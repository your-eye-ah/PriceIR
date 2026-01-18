#include <furi.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/view.h>
#include <gui/modules/submenu.h>
#include <gui/modules/variable_item_list.h>
#include <gui/modules/byte_input.h>
#include <gui/modules/text_input.h>
#include <gui/modules/widget.h>
#include <dialogs/dialogs.h>
#include <storage/storage.h>
#include <input/input.h>
#include <stdio.h>
#include <string.h>
#include "precir_protocol.h"
#include "precir_driver.h"

// Default Label ID to target
#define DEFAULT_PLID_STR "A4108251199313541" 

typedef enum {
    PrecIRViewMain,
    PrecIRViewMenu,
    PrecIRViewConfig, // Mode
    PrecIRViewIdMenu,
    PrecIRViewIdHex,
    PrecIRViewIdText,
    PrecIRViewAbout,
} PrecIRView;

typedef enum {
    PrecIRModePing,
    PrecIRModeRefresh,
} PrecIRAction;

typedef enum {
    PrecIRProtoPP4,
    PrecIRProtoPP16,
} PrecIRProtocol;

typedef struct {
    PrecIrLabelId current_id;
    PrecIRAction action;
    PrecIRProtocol protocol;
    bool transmitting;
} PrecIRModel;

typedef struct {
    ViewDispatcher* view_dispatcher;
    Gui* gui;
    DialogsApp* dialogs;
    
    // Views
    View* view_main;
    Submenu* submenu_main;
    VariableItemList* var_list_config;
    Submenu* submenu_id;
    ByteInput* byte_input;
    TextInput* text_input;
    Widget* widget_about;

    // State
    char text_input_buf[24];
    PrecIrLabelId temp_id_buf; // Buffer for async editing
} PrecIRApp;


// --- Main View ---

static void precir_draw_callback(Canvas* canvas, void* model_data) {
    PrecIRModel* model = (PrecIRModel*)model_data;
    
    canvas_clear(canvas);
    canvas_set_font(canvas, FontSecondary);
    
    // Header
    canvas_draw_str(canvas, 2, 8, "FlipIR Transmitter");

    // ID
    char buf[64];
    snprintf(buf, sizeof(buf), "ID: %02X %02X %02X %02X", 
        model->current_id.id[0], model->current_id.id[1], 
        model->current_id.id[2], model->current_id.id[3]);
    canvas_draw_str(canvas, 2, 20, buf);
    
    // Settings
    const char* proto_str = (model->protocol == PrecIRProtoPP16) ? "PP16" : "PP4";
    const char* action_str = (model->action == PrecIRModeRefresh) ? "Refresh" : "Ping";
    snprintf(buf, sizeof(buf), "%s - %s", proto_str, action_str);
    canvas_draw_str(canvas, 2, 32, buf);
    
    // Status
    if(model->transmitting) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 48, AlignCenter, AlignCenter, "TRANSMITTING...");
    } else {
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 52, AlignCenter, AlignBottom, "Press OK to TX");
        canvas_draw_str_aligned(canvas, 126, 62, AlignRight, AlignBottom, "Menu >");
    }
}

static void precir_transmit_process(PrecIRApp* app) {
    // Lock Model, Set Transmitting, Unlock(Update)
    {
        PrecIRModel* m = view_get_model(app->view_main);
        m->transmitting = true;
        view_commit_model(app->view_main, true);
    }

    // Lock Model, Copy Data, Unlock(NoUpdate) - Quick copy to avoid holding lock during TX
    PrecIRModel m_copy;
    {
        PrecIRModel* m = view_get_model(app->view_main);
        m_copy = *m;
        view_commit_model(app->view_main, false);
    }

    uint8_t frame[PRECIR_MAX_FRAME_SIZE];
    size_t len = 0;
    memset(frame, 0, PRECIR_MAX_FRAME_SIZE);

    bool pp16 = (m_copy.protocol == PrecIRProtoPP16);
    
    if(m_copy.action == PrecIRModePing) {
        precir_create_ping_frame(frame, &len, &m_copy.current_id, pp16, 200);
    } else {
        precir_create_refresh_frame(frame, &len, &m_copy.current_id, pp16);
    }

    precir_driver_transmit(frame, len, pp16);

    // Lock Model, Clear Transmitting, Unlock(Update)
    {
        PrecIRModel* m = view_get_model(app->view_main);
        m->transmitting = false;
        view_commit_model(app->view_main, true);
    }
}

static void precir_transmit_file(PrecIRApp* app) {
    FuriString* file_path = furi_string_alloc();
    furi_string_set(file_path, "/ext");

    DialogsFileBrowserOptions browser_options;
    dialog_file_browser_set_basic_options(&browser_options, ".esl", NULL);
    browser_options.hide_ext = false;

    bool res = dialog_file_browser_show(app->dialogs, file_path, file_path, &browser_options);
    if(res) {
        // Show transmitting UI
        {
            PrecIRModel* m = view_get_model(app->view_main);
            m->transmitting = true;
            view_commit_model(app->view_main, true);
            view_dispatcher_switch_to_view(app->view_dispatcher, PrecIRViewMain);
        }

        Storage* storage = furi_record_open(RECORD_STORAGE);
        File* file = storage_file_alloc(storage);
        if(storage_file_open(file, furi_string_get_cstr(file_path), FSAM_READ, FSOM_OPEN_EXISTING)) {

            uint8_t header = 0;
            if(storage_file_read(file, &header, 1) == 1) {
                bool pp16 = (header & 0x01);

                uint8_t len_buf[1];
                uint8_t repeats_buf[2];
                uint8_t frame_buf[PRECIR_MAX_FRAME_SIZE];

                while(storage_file_read(file, repeats_buf, 2) == 2) {
                     if(storage_file_read(file, len_buf, 1) != 1) break;
                     uint8_t len = len_buf[0];

                     if(len > PRECIR_MAX_FRAME_SIZE) len = PRECIR_MAX_FRAME_SIZE;

                     if(storage_file_read(file, frame_buf, len) != len) break;

                     uint16_t repeats = repeats_buf[0] | (repeats_buf[1] << 8);

                     for(uint16_t r=0; r<repeats; r++) {
                         precir_driver_transmit(frame_buf, len, pp16);
                         if(repeats > 1) furi_delay_us(1000);
                     }
                     furi_delay_ms(10);
                }
            }
            storage_file_close(file);
        }
        storage_file_free(file);
        furi_record_close(RECORD_STORAGE);

        // Clear transmitting UI
        {
            PrecIRModel* m = view_get_model(app->view_main);
            m->transmitting = false;
            view_commit_model(app->view_main, true);
        }
    }
    furi_string_free(file_path);
}

static bool precir_input_callback(InputEvent* event, void* context) {
    PrecIRApp* app = (PrecIRApp*)context;
    
    if(event->type == InputTypeShort) {
        if(event->key == InputKeyOk) {
            precir_transmit_process(app);
            return true;
        } else if(event->key == InputKeyRight) {
            view_dispatcher_switch_to_view(app->view_dispatcher, PrecIRViewMenu);
            return true;
        }
    }
    return false;
}

// --- Menu Helpers ---

static void precir_menu_callback(void* context, uint32_t index) {
    PrecIRApp* app = (PrecIRApp*)context;
    switch(index) {
        case 0: view_dispatcher_switch_to_view(app->view_dispatcher, PrecIRViewConfig); break;
        case 1: view_dispatcher_switch_to_view(app->view_dispatcher, PrecIRViewIdMenu); break;
        case 2: precir_transmit_file(app); break;
        case 3: view_dispatcher_switch_to_view(app->view_dispatcher, PrecIRViewAbout); break;
    }
}

// --- Config Helpers ---

static void precir_config_change_proto(VariableItem* item) {
    PrecIRApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    
    PrecIRModel* m = view_get_model(app->view_main);
    m->protocol = (index == 0) ? PrecIRProtoPP4 : PrecIRProtoPP16;
    variable_item_set_current_value_text(item, (index == 0) ? "PP4" : "PP16");
    view_commit_model(app->view_main, true);
}

static void precir_config_change_action(VariableItem* item) {
    PrecIRApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    
    PrecIRModel* m = view_get_model(app->view_main);
    m->action = (index == 0) ? PrecIRModePing : PrecIRModeRefresh;
    variable_item_set_current_value_text(item, (index == 0) ? "Ping" : "Refresh");
    view_commit_model(app->view_main, true);
}

// --- ID Entry Callbacks ---

static void precir_hex_input_result(void* context) {
    PrecIRApp* app = (PrecIRApp*)context;
    
    // Copy temp_id to model
    {
        PrecIRModel* m = view_get_model(app->view_main);
        memcpy(&m->current_id, &app->temp_id_buf, sizeof(PrecIrLabelId));
        view_commit_model(app->view_main, true);
    }
    
    view_dispatcher_switch_to_view(app->view_dispatcher, PrecIRViewIdMenu);
}

static void precir_text_input_result(void* context) {
    PrecIRApp* app = (PrecIRApp*)context;
    
    PrecIrLabelId new_id;
    if(precir_parse_plid(app->text_input_buf, &new_id)) {
        PrecIRModel* m = view_get_model(app->view_main);
        memcpy(&m->current_id, &new_id, sizeof(PrecIrLabelId));
        view_commit_model(app->view_main, true);
    }
    
    view_dispatcher_switch_to_view(app->view_dispatcher, PrecIRViewIdMenu);
}

// --- ID Menu Callback ---

static void precir_id_menu_callback(void* context, uint32_t index) {
    PrecIRApp* app = (PrecIRApp*)context;
    switch(index) {
        case 0: // Hex
            { 
                 // Copy current ID to temp buffer for editing
                 // Lock model briefly to read
                 {
                     PrecIRModel* m = view_get_model(app->view_main);
                     app->temp_id_buf = m->current_id;
                     view_commit_model(app->view_main, false);
                 }
                 
                 byte_input_set_result_callback(app->byte_input, 
                    precir_hex_input_result, NULL, app, (void*)app->temp_id_buf.id, 4);
                 view_dispatcher_switch_to_view(app->view_dispatcher, PrecIRViewIdHex); 
            }
            break;
        case 1: // Barcode
            {
                // Clear buf
                app->text_input_buf[0] = '\0';
                text_input_set_result_callback(app->text_input, 
                    precir_text_input_result, app, app->text_input_buf, sizeof(app->text_input_buf), false);
                view_dispatcher_switch_to_view(app->view_dispatcher, PrecIRViewIdText);
            }
            break;
    }
}

// --- Setup ---

static uint32_t precir_back_to_main_callback(void* context) {
    UNUSED(context);
    return PrecIRViewMain;
}

static uint32_t precir_back_to_menu_callback(void* context) {
    UNUSED(context);
    return PrecIRViewMenu;
}

static uint32_t precir_back_to_id_menu_callback(void* context) {
    UNUSED(context);
    return PrecIRViewIdMenu;
}

void precir_setup_default_id(PrecIRApp* app) {
    PrecIRModel* m = view_get_model(app->view_main);
    if(precir_parse_plid(DEFAULT_PLID_STR, &m->current_id)) {
         // ok
    }
    m->protocol = PrecIRProtoPP4;
    m->action = PrecIRModePing;
    m->transmitting = false;
    view_commit_model(app->view_main, true);
}

static uint32_t precir_exit_callback(void* context) {
    UNUSED(context);
    return VIEW_NONE; 
}

int32_t flip_precir_app(void* p) {
    UNUSED(p);
    PrecIRApp* app = malloc(sizeof(PrecIRApp));
    memset(app, 0, sizeof(PrecIRApp));

    app->gui = furi_record_open(RECORD_GUI);
    app->dialogs = furi_record_open(RECORD_DIALOGS);
    app->view_dispatcher = view_dispatcher_alloc();
    
    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    // 1. Main View (Custom)
    app->view_main = view_alloc();
    view_set_context(app->view_main, app);
    view_allocate_model(app->view_main, ViewModelTypeLocking, sizeof(PrecIRModel));
    view_set_draw_callback(app->view_main, precir_draw_callback);
    view_set_input_callback(app->view_main, precir_input_callback);
    
    view_set_previous_callback(app->view_main, precir_exit_callback);
    
    view_dispatcher_add_view(app->view_dispatcher, PrecIRViewMain, app->view_main);

    // 2. Menu
    app->submenu_main = submenu_alloc();
    submenu_add_item(app->submenu_main, "Mode / Config", 0, precir_menu_callback, app);
    submenu_add_item(app->submenu_main, "Set ID", 1, precir_menu_callback, app);
    submenu_add_item(app->submenu_main, "Transmit File", 2, precir_menu_callback, app);
    submenu_add_item(app->submenu_main, "About", 3, precir_menu_callback, app);
    view_set_previous_callback(submenu_get_view(app->submenu_main), precir_back_to_main_callback);
    view_dispatcher_add_view(app->view_dispatcher, PrecIRViewMenu, submenu_get_view(app->submenu_main));

    // 3. Config (Variable List)
    app->var_list_config = variable_item_list_alloc();
    variable_item_list_reset(app->var_list_config);
    
    // Proto
    VariableItem* item = variable_item_list_add(app->var_list_config, "Protocol", 2, precir_config_change_proto, app);
    variable_item_set_current_value_index(item, 0); // Default PP4
    variable_item_set_current_value_text(item, "PP4");
    
    // Action
    item = variable_item_list_add(app->var_list_config, "Action", 2, precir_config_change_action, app);
    variable_item_set_current_value_index(item, 0); // Default Ping
    variable_item_set_current_value_text(item, "Ping");

    view_set_previous_callback(variable_item_list_get_view(app->var_list_config), precir_back_to_menu_callback);
    view_dispatcher_add_view(app->view_dispatcher, PrecIRViewConfig, variable_item_list_get_view(app->var_list_config));

    // 4. ID Menu
    app->submenu_id = submenu_alloc();
    submenu_add_item(app->submenu_id, "Enter PLID (Hex)", 0, precir_id_menu_callback, app);
    submenu_add_item(app->submenu_id, "Enter Barcode", 1, precir_id_menu_callback, app);
    view_set_previous_callback(submenu_get_view(app->submenu_id), precir_back_to_menu_callback); 
    view_dispatcher_add_view(app->view_dispatcher, PrecIRViewIdMenu, submenu_get_view(app->submenu_id));

    // 5. Hex Input
    app->byte_input = byte_input_alloc();
    byte_input_set_header_text(app->byte_input, "Set PLID (4 Bytes)");
    view_set_previous_callback(byte_input_get_view(app->byte_input), precir_back_to_id_menu_callback); 
    view_dispatcher_add_view(app->view_dispatcher, PrecIRViewIdHex, byte_input_get_view(app->byte_input));

    // 6. Text Input
    app->text_input = text_input_alloc();
    text_input_set_header_text(app->text_input, "Enter Barcode");
    view_set_previous_callback(text_input_get_view(app->text_input), precir_back_to_id_menu_callback);
    view_dispatcher_add_view(app->view_dispatcher, PrecIRViewIdText, text_input_get_view(app->text_input));
    
    // 7. About
    app->widget_about = widget_alloc();
    widget_add_text_scroll_element(app->widget_about, 0, 0, 128, 64, 
        "FlipIR Pricer Tool\n\nControl Electronic Shelf Labels.\n\nCreated by Your_eye_ah\n\nImplements PP4/PP16 protocols.");
    view_set_previous_callback(widget_get_view(app->widget_about), precir_back_to_menu_callback);
    view_dispatcher_add_view(app->view_dispatcher, PrecIRViewAbout, widget_get_view(app->widget_about));


    // Init Data
    precir_setup_default_id(app);
    precir_driver_init();

    // Start
    view_dispatcher_switch_to_view(app->view_dispatcher, PrecIRViewMain);

    // Loop
    view_dispatcher_run(app->view_dispatcher);

    // Cleanup
    precir_driver_deinit();
    
    view_dispatcher_remove_view(app->view_dispatcher, PrecIRViewMain);
    view_dispatcher_remove_view(app->view_dispatcher, PrecIRViewMenu);
    view_dispatcher_remove_view(app->view_dispatcher, PrecIRViewConfig);
    view_dispatcher_remove_view(app->view_dispatcher, PrecIRViewIdMenu);
    view_dispatcher_remove_view(app->view_dispatcher, PrecIRViewIdHex);
    view_dispatcher_remove_view(app->view_dispatcher, PrecIRViewIdText);
    view_dispatcher_remove_view(app->view_dispatcher, PrecIRViewAbout);

    view_free(app->view_main);
    submenu_free(app->submenu_main);
    variable_item_list_free(app->var_list_config);
    submenu_free(app->submenu_id);
    byte_input_free(app->byte_input);
    text_input_free(app->text_input);
    widget_free(app->widget_about);
    
    view_dispatcher_free(app->view_dispatcher);
    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_DIALOGS);
    free(app);

    return 0;
}
