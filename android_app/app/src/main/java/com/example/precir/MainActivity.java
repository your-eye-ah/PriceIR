package com.example.precir;

import android.app.Activity;
import android.content.Intent;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Color;
import android.net.Uri;
import android.os.Bundle;
import android.provider.MediaStore;
import android.view.View;
import android.widget.Button;
import android.widget.CheckBox;
import android.widget.EditText;
import android.widget.ImageView;
import android.widget.TextView;
import android.widget.Toast;

import androidx.activity.result.ActivityResultLauncher;
import androidx.activity.result.contract.ActivityResultContracts;
import androidx.appcompat.app.AppCompatActivity;

import java.io.OutputStream;
import java.io.InputStream;

public class MainActivity extends AppCompatActivity {

    private ImageView imageViewPreview;
    private EditText editTextBarcode;
    private CheckBox checkBoxPP16;
    private TextView textViewStatus;
    private Button buttonSave;

    private Bitmap currentBitmap;
    private byte[] generatedData;

    private final ActivityResultLauncher<String> pickImageLauncher = registerForActivityResult(
            new ActivityResultContracts.GetContent(),
            uri -> {
                if (uri != null) {
                    loadImage(uri);
                }
            });

    private final ActivityResultLauncher<String> createDocumentLauncher = registerForActivityResult(
            new ActivityResultContracts.CreateDocument(),
            uri -> {
                if (uri != null) {
                    saveFile(uri);
                }
            });

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        Button buttonLoad = findViewById(R.id.buttonLoad);
        imageViewPreview = findViewById(R.id.imageViewPreview);
        editTextBarcode = findViewById(R.id.editTextBarcode);
        checkBoxPP16 = findViewById(R.id.checkBoxPP16);
        buttonSave = findViewById(R.id.buttonSave);
        textViewStatus = findViewById(R.id.textViewStatus);

        buttonLoad.setOnClickListener(v -> pickImageLauncher.launch("image/*"));

        buttonSave.setOnClickListener(v -> {
            if (currentBitmap == null) {
                Toast.makeText(this, "Please load an image first", Toast.LENGTH_SHORT).show();
                return;
            }
            generateData();
            createDocumentLauncher.launch("image.esl");
        });
    }

    private void loadImage(Uri uri) {
        try {
            InputStream inputStream = getContentResolver().openInputStream(uri);
            currentBitmap = BitmapFactory.decodeStream(inputStream);
            imageViewPreview.setImageBitmap(currentBitmap);
            textViewStatus.setText("Image loaded: " + currentBitmap.getWidth() + "x" + currentBitmap.getHeight());
            buttonSave.setEnabled(true);
        } catch (Exception e) {
            e.printStackTrace();
            Toast.makeText(this, "Failed to load image", Toast.LENGTH_SHORT).show();
        }
    }

    private void generateData() {
        if (currentBitmap == null) return;

        String barcode = editTextBarcode.getText().toString();
        boolean pp16 = checkBoxPP16.isChecked();

        int width = currentBitmap.getWidth();
        int height = currentBitmap.getHeight();
        int[] pixels = new int[width * height];
        currentBitmap.getPixels(pixels, 0, width, 0, 0, width, height);

        // Ensure width/height multiple of 8?
        // Logic handled in encoder or pre-processing?
        // EslEncoder doesn't enforce it but protocol might need it.
        // I'll leave it raw for now, assuming user knows or Encoder handles it (it doesn't).
        // Let's warn if not.
        if (width % 8 != 0 || height % 8 != 0) {
            Toast.makeText(this, "Warning: Dimensions should be multiple of 8", Toast.LENGTH_LONG).show();
        }

        generatedData = EslEncoder.encode(pixels, width, height, barcode, pp16, false);
    }

    private void saveFile(Uri uri) {
        if (generatedData == null) return;
        try {
            OutputStream outputStream = getContentResolver().openOutputStream(uri);
            if (outputStream != null) {
                outputStream.write(generatedData);
                outputStream.close();
                Toast.makeText(this, "File saved successfully", Toast.LENGTH_SHORT).show();
                textViewStatus.setText("Saved .esl file");
            }
        } catch (Exception e) {
            e.printStackTrace();
            Toast.makeText(this, "Failed to save file", Toast.LENGTH_SHORT).show();
        }
    }
}
