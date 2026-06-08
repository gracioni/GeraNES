package org.libsdl.app;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.os.Build;
import android.os.Bundle;
import android.view.View;
import android.view.WindowInsets;
import android.view.WindowInsetsController;
import android.view.WindowManager;
import android.webkit.WebSettings;
import android.webkit.WebView;
import android.webkit.WebViewClient;

import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.nio.charset.StandardCharsets;

public class DocumentationActivity extends Activity {
    private static final String DOCS_ASSET_PATH = "docs/index.html";
    private static final String DOCS_ASSET_BASE_URL = "file:///android_asset/docs/";
    private WebView webView;

    @Override
    @SuppressLint("SetJavaScriptEnabled")
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        webView = new WebView(this);
        final WebSettings settings = webView.getSettings();
        settings.setAllowFileAccess(true);
        settings.setAllowContentAccess(true);
        settings.setDomStorageEnabled(true);
        settings.setJavaScriptEnabled(true);
        if(Build.VERSION.SDK_INT >= Build.VERSION_CODES.JELLY_BEAN) {
            settings.setAllowFileAccessFromFileURLs(true);
            settings.setAllowUniversalAccessFromFileURLs(true);
        }
        webView.setWebViewClient(new WebViewClient());
        setContentView(webView);
        applyImmersiveFullscreen();
        loadDocumentation();
    }

    @Override
    protected void onResume() {
        super.onResume();
        applyImmersiveFullscreen();
    }

    @Override
    public void onBackPressed() {
        if(webView != null && webView.canGoBack()) {
            webView.goBack();
            return;
        }
        super.onBackPressed();
    }

    private void loadDocumentation() {
        try {
            final String html = readAllText(getAssets().open(DOCS_ASSET_PATH));
            webView.loadDataWithBaseURL(
                DOCS_ASSET_BASE_URL,
                html,
                "text/html",
                "UTF-8",
                null
            );
            return;
        } catch(IOException ignored) {
        }

        final File runtimeDoc = new File(new File(getFilesDir(), "runtime_data"), "docs/index.html");
        if(runtimeDoc.isFile()) {
            try {
                final String html = readAllText(new FileInputStream(runtimeDoc));
                final String baseUrl = runtimeDoc.getParentFile().toURI().toString();
                webView.loadDataWithBaseURL(
                    baseUrl.endsWith("/") ? baseUrl : (baseUrl + "/"),
                    html,
                    "text/html",
                    "UTF-8",
                    null
                );
                return;
            } catch(IOException ignored) {
            }
        }

        webView.loadData(
            "<html><body><h2>Documentation not found</h2><p>The bundled help files could not be loaded.</p></body></html>",
            "text/html",
            "UTF-8"
        );
    }

    private String readAllText(InputStream inputStream) throws IOException {
        try (InputStream in = inputStream; ByteArrayOutputStream out = new ByteArrayOutputStream()) {
            final byte[] buffer = new byte[16 * 1024];
            int read;
            while((read = in.read(buffer)) != -1) {
                out.write(buffer, 0, read);
            }
            return out.toString(StandardCharsets.UTF_8.name());
        }
    }

    private void applyImmersiveFullscreen() {
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN);
        if(Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            getWindow().setDecorFitsSystemWindows(false);
            final WindowInsetsController controller = getWindow().getInsetsController();
            if(controller != null) {
                controller.hide(WindowInsets.Type.systemBars());
                controller.setSystemBarsBehavior(WindowInsetsController.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE);
            }
        } else {
            final View decorView = getWindow().getDecorView();
            decorView.setSystemUiVisibility(
                View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY
                    | View.SYSTEM_UI_FLAG_LAYOUT_STABLE
                    | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
                    | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
                    | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
                    | View.SYSTEM_UI_FLAG_FULLSCREEN
            );
        }
    }
}
