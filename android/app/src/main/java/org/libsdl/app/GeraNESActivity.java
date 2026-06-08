package org.libsdl.app;

import android.content.pm.ActivityInfo;
import android.content.res.Configuration;
import android.content.res.AssetManager;
import android.content.ContentResolver;
import android.content.Intent;
import android.database.Cursor;
import android.net.Uri;
import android.os.Build;
import android.provider.DocumentsContract;
import android.provider.OpenableColumns;
import android.util.Log;
import android.view.View;
import android.view.WindowInsets;
import android.view.WindowInsetsController;
import android.view.WindowManager;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import org.json.JSONObject;

public class GeraNESActivity extends SDLActivity {
    private static final String TAG = "GeraNESActivity";
    private static final int REQUEST_OPEN_DOCUMENT_BASE = 0x4700;
    private static final int REQUEST_OPEN_TREE_BASE = 0x4A00;
    private static final int REQUEST_CREATE_DOCUMENT_BASE = 0x4D00;

    private static native void nativeOnOpenPickerResult(int requestId, String localPath, String error);
    private static native void nativeOnCreateDocumentResult(int requestId, String uri, String error);

    @Override
    protected void onCreate(android.os.Bundle savedInstanceState) {
        try {
            syncBundledRuntimeData();
        } catch (Exception ignored) {
        }
        super.onCreate(savedInstanceState);
        setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_FULL_USER);
        applyImmersiveFullscreen();
    }

    @Override
    protected void onResume() {
        super.onResume();
        applyImmersiveFullscreen();
    }

    @Override
    public void onConfigurationChanged(Configuration newConfig) {
        super.onConfigurationChanged(newConfig);
        applyImmersiveFullscreen();
    }

    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);
        if(hasFocus) {
            applyImmersiveFullscreen();
        }
    }

    public boolean geranesOpenBundledDocumentation() {
        try {
            final Intent intent = new Intent(this, DocumentationActivity.class);
            startActivity(intent);
            return true;
        } catch(Exception e) {
            Log.e(TAG, "Failed to open bundled documentation", e);
            return false;
        }
    }

    public boolean geranesEnsureRuntimeDataSynced() {
        try {
            syncBundledRuntimeData();
            return true;
        } catch(Exception e) {
            Log.e(TAG, "Failed to sync bundled runtime data", e);
            return false;
        }
    }

    public void geranesShowOpenDocumentPicker(final int requestId, final String[] mimeTypes) {
        runOnUiThread(() -> {
            final Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT);
            intent.addCategory(Intent.CATEGORY_OPENABLE);
            intent.setType("*/*");
            intent.putExtra(Intent.EXTRA_ALLOW_MULTIPLE, false);
            if(mimeTypes != null && mimeTypes.length > 0) {
                intent.putExtra(Intent.EXTRA_MIME_TYPES, mimeTypes);
            }
            startActivityForResult(intent, REQUEST_OPEN_DOCUMENT_BASE + requestId);
        });
    }

    public void geranesShowOpenTreePicker(final int requestId) {
        runOnUiThread(() -> {
            final Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT_TREE);
            startActivityForResult(intent, REQUEST_OPEN_TREE_BASE + requestId);
        });
    }

    public void geranesShowCreateDocumentPicker(final int requestId, final String mimeType, final String suggestedName) {
        runOnUiThread(() -> {
            final Intent intent = new Intent(Intent.ACTION_CREATE_DOCUMENT);
            intent.addCategory(Intent.CATEGORY_OPENABLE);
            intent.setType((mimeType == null || mimeType.isEmpty()) ? "*/*" : mimeType);
            intent.putExtra(Intent.EXTRA_TITLE, (suggestedName == null || suggestedName.isEmpty()) ? "file.bin" : suggestedName);
            startActivityForResult(intent, REQUEST_CREATE_DOCUMENT_BASE + requestId);
        });
    }

    public boolean geranesWriteBytesToUri(String uriString, byte[] bytes) {
        if(uriString == null || uriString.isEmpty()) {
            return false;
        }

        OutputStream outputStream = null;
        try {
            outputStream = getContentResolver().openOutputStream(Uri.parse(uriString), "w");
            if(outputStream == null) {
                return false;
            }
            if(bytes != null && bytes.length > 0) {
                outputStream.write(bytes);
            }
            outputStream.flush();
            return true;
        } catch(IOException e) {
            return false;
        } finally {
            if(outputStream != null) {
                try {
                    outputStream.close();
                } catch(IOException ignored) {
                }
            }
        }
    }

    public String geranesCopyDocumentUriToCache(String uriString) {
        if(uriString == null || uriString.isEmpty()) {
            return null;
        }

        try {
            final Uri uri = Uri.parse(uriString);
            final String displayName = chooseDisplayName(uri);
            final File target = new File(createTransientCacheDirectory("recent-file"), displayName);
            copyUriToFile(uri, target);

            final JSONObject payload = new JSONObject();
            payload.put("cachePath", target.getAbsolutePath());
            payload.put("displayName", displayName);
            payload.put("uri", uri.toString());
            return payload.toString();
        } catch(Exception e) {
            Log.e(TAG, "Failed copying persisted document URI to cache", e);
            return null;
        }
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        applyImmersiveFullscreen();

        if(requestCode >= REQUEST_OPEN_DOCUMENT_BASE && requestCode < REQUEST_OPEN_TREE_BASE) {
            final int requestId = requestCode - REQUEST_OPEN_DOCUMENT_BASE;
            handleOpenDocumentResult(requestId, resultCode, data);
            return;
        }
        if(requestCode >= REQUEST_OPEN_TREE_BASE && requestCode < REQUEST_CREATE_DOCUMENT_BASE) {
            final int requestId = requestCode - REQUEST_OPEN_TREE_BASE;
            handleOpenTreeResult(requestId, resultCode, data);
            return;
        }
        if(requestCode >= REQUEST_CREATE_DOCUMENT_BASE) {
            final int requestId = requestCode - REQUEST_CREATE_DOCUMENT_BASE;
            handleCreateDocumentResult(requestId, resultCode, data);
        }
    }

    private void handleOpenDocumentResult(final int requestId, final int resultCode, final Intent data) {
        if(resultCode != RESULT_OK || data == null || data.getData() == null) {
            Log.i(TAG, "Open document picker cancelled for request " + requestId);
            nativeOnOpenPickerResult(requestId, null, null);
            return;
        }

        final Uri uri = data.getData();
        Log.i(TAG, "Open document picker returned URI " + uri + " for request " + requestId);
        takePersistableReadPermission(uri, data);
        new Thread(() -> {
            try {
                final File target = new File(createRequestCacheDirectory("picked-files", requestId), chooseDisplayName(uri));
                copyUriToFile(uri, target);
                Log.i(TAG, "Copied picked document to " + target.getAbsolutePath());
                final JSONObject payload = new JSONObject();
                payload.put("cachePath", target.getAbsolutePath());
                payload.put("displayName", chooseDisplayName(uri));
                payload.put("uri", uri.toString());
                nativeOnOpenPickerResult(requestId, payload.toString(), null);
            } catch(Exception e) {
                Log.e(TAG, "Failed copying picked document", e);
                nativeOnOpenPickerResult(requestId, null, e.getMessage());
            }
        }, "GeraNES-OpenDocument").start();
    }

    private void handleOpenTreeResult(final int requestId, final int resultCode, final Intent data) {
        if(resultCode != RESULT_OK || data == null || data.getData() == null) {
            Log.i(TAG, "Open tree picker cancelled for request " + requestId);
            nativeOnOpenPickerResult(requestId, null, null);
            return;
        }

        final Uri treeUri = data.getData();
        Log.i(TAG, "Open tree picker returned URI " + treeUri + " for request " + requestId);
        takePersistableReadPermission(treeUri, data);
        new Thread(() -> {
            try {
                final File targetDir = createRequestCacheDirectory("picked-tree", requestId);
                copyTreeChildren(treeUri, DocumentsContract.getTreeDocumentId(treeUri), targetDir);
                Log.i(TAG, "Copied picked tree to " + targetDir.getAbsolutePath());
                nativeOnOpenPickerResult(requestId, targetDir.getAbsolutePath(), null);
            } catch(Exception e) {
                Log.e(TAG, "Failed copying picked tree", e);
                nativeOnOpenPickerResult(requestId, null, e.getMessage());
            }
        }, "GeraNES-OpenTree").start();
    }

    private void handleCreateDocumentResult(final int requestId, final int resultCode, final Intent data) {
        if(resultCode != RESULT_OK || data == null || data.getData() == null) {
            Log.i(TAG, "Create document picker cancelled for request " + requestId);
            nativeOnCreateDocumentResult(requestId, null, null);
            return;
        }

        final Uri uri = data.getData();
        Log.i(TAG, "Create document picker returned URI " + uri + " for request " + requestId);
        final int flags = data.getFlags();
        final int persistFlags = flags & (Intent.FLAG_GRANT_READ_URI_PERMISSION | Intent.FLAG_GRANT_WRITE_URI_PERMISSION);
        if(persistFlags != 0) {
            try {
                getContentResolver().takePersistableUriPermission(uri, persistFlags);
            } catch(SecurityException ignored) {
            }
        }
        nativeOnCreateDocumentResult(requestId, uri.toString(), null);
    }

    private void takePersistableReadPermission(Uri uri, Intent data) {
        final int flags = data.getFlags();
        final int persistFlags = flags & (Intent.FLAG_GRANT_READ_URI_PERMISSION | Intent.FLAG_GRANT_WRITE_URI_PERMISSION);
        if(persistFlags != 0) {
            try {
                getContentResolver().takePersistableUriPermission(uri, persistFlags);
            } catch(SecurityException ignored) {
            }
        }
    }

    private File createRequestCacheDirectory(String category, int requestId) throws IOException {
        final File root = new File(getCacheDir(), "geranes-picker/" + category + "-" + requestId);
        deleteRecursively(root);
        if(!root.mkdirs() && !root.isDirectory()) {
            throw new IOException("Could not create picker cache directory.");
        }
        return root;
    }

    private File createTransientCacheDirectory(String category) throws IOException {
        final File root = new File(getCacheDir(), "geranes-picker/" + category + "-" + System.currentTimeMillis());
        deleteRecursively(root);
        if(!root.mkdirs() && !root.isDirectory()) {
            throw new IOException("Could not create picker cache directory.");
        }
        return root;
    }

    private void copyUriToFile(Uri sourceUri, File targetFile) throws IOException {
        final File parent = targetFile.getParentFile();
        if(parent != null && !parent.exists() && !parent.mkdirs() && !parent.isDirectory()) {
            throw new IOException("Could not create parent directory for " + targetFile.getName());
        }

        final ContentResolver resolver = getContentResolver();
        try (InputStream inputStream = resolver.openInputStream(sourceUri);
             OutputStream outputStream = new FileOutputStream(targetFile, false)) {
            if(inputStream == null) {
                throw new IOException("Could not open source document.");
            }
            final byte[] buffer = new byte[64 * 1024];
            int read;
            while((read = inputStream.read(buffer)) != -1) {
                outputStream.write(buffer, 0, read);
            }
        }
    }

    private void copyTreeChildren(Uri treeUri, String documentId, File targetDir) throws IOException {
        final Uri childrenUri = DocumentsContract.buildChildDocumentsUriUsingTree(treeUri, documentId);
        final String[] projection = new String[] {
            DocumentsContract.Document.COLUMN_DOCUMENT_ID,
            DocumentsContract.Document.COLUMN_DISPLAY_NAME,
            DocumentsContract.Document.COLUMN_MIME_TYPE
        };

        try (Cursor cursor = getContentResolver().query(childrenUri, projection, null, null, null)) {
            if(cursor == null) {
                throw new IOException("Could not enumerate folder contents.");
            }

            while(cursor.moveToNext()) {
                final String childDocumentId = cursor.getString(0);
                final String displayName = sanitizeFileName(cursor.getString(1));
                final String mimeType = cursor.getString(2);
                final Uri childUri = DocumentsContract.buildDocumentUriUsingTree(treeUri, childDocumentId);
                final File childTarget = new File(targetDir, displayName);

                if(DocumentsContract.Document.MIME_TYPE_DIR.equals(mimeType)) {
                    if(!childTarget.mkdirs() && !childTarget.isDirectory()) {
                        throw new IOException("Could not create directory " + childTarget.getName());
                    }
                    copyTreeChildren(treeUri, childDocumentId, childTarget);
                } else {
                    copyUriToFile(childUri, childTarget);
                }
            }
        }
    }

    private String chooseDisplayName(Uri uri) {
        final String queriedName = queryDisplayName(uri);
        if(queriedName != null && !queriedName.isEmpty()) {
            return sanitizeFileName(queriedName);
        }
        final String lastPath = uri.getLastPathSegment();
        if(lastPath != null && !lastPath.isEmpty()) {
            return sanitizeFileName(lastPath);
        }
        return "picked-file.bin";
    }

    private String queryDisplayName(Uri uri) {
        try (Cursor cursor = getContentResolver().query(uri, new String[] { OpenableColumns.DISPLAY_NAME }, null, null, null)) {
            if(cursor != null && cursor.moveToFirst()) {
                return cursor.getString(0);
            }
        } catch(Exception ignored) {
        }
        return null;
    }

    private String sanitizeFileName(String name) {
        if(name == null || name.isEmpty()) {
            return "unnamed";
        }
        return name.replaceAll("[\\\\/:*?\"<>|]", "_");
    }

    private void deleteRecursively(File file) {
        if(file == null || !file.exists()) {
            return;
        }
        if(file.isDirectory()) {
            final File[] children = file.listFiles();
            if(children != null) {
                for(File child : children) {
                    deleteRecursively(child);
                }
            }
        }
        file.delete();
    }

    private void syncBundledRuntimeData() throws IOException {
        final File runtimeRoot = new File(getFilesDir(), "runtime_data");
        if(!runtimeRoot.exists() && !runtimeRoot.mkdirs() && !runtimeRoot.isDirectory()) {
            throw new IOException("Could not create runtime data directory.");
        }

        syncAssetChildrenToDirectory("", runtimeRoot);
        syncAssetTree("docs", new File(runtimeRoot, "docs"));
    }

    private void syncAssetChildrenToDirectory(String assetPath, File targetDir) throws IOException {
        final String[] entries = getAssets().list(assetPath);
        if(entries == null || entries.length == 0) {
            return;
        }

        if(!targetDir.exists() && !targetDir.mkdirs() && !targetDir.isDirectory()) {
            throw new IOException("Could not create directory " + targetDir.getAbsolutePath());
        }

        for(String entry : entries) {
            if(entry == null || entry.isEmpty() || entry.equals("docs")) {
                continue;
            }
            final String childAssetPath = assetPath.isEmpty() ? entry : assetPath + "/" + entry;
            syncAssetTree(childAssetPath, new File(targetDir, entry));
        }
    }

    private void syncAssetTree(String assetPath, File targetPath) throws IOException {
        final AssetManager assets = getAssets();
        final String[] entries = assets.list(assetPath);
        if(entries == null || entries.length == 0) {
            try {
                copySingleAsset(assetPath, targetPath);
            } catch(IOException e) {
                if(assetPath.startsWith("docs")) {
                    return;
                }
                throw e;
            }
            return;
        }

        if(!targetPath.exists() && !targetPath.mkdirs() && !targetPath.isDirectory()) {
            throw new IOException("Could not create directory " + targetPath.getAbsolutePath());
        }

        for(String entry : entries) {
            final String childAssetPath = assetPath.isEmpty() ? entry : assetPath + "/" + entry;
            syncAssetTree(childAssetPath, new File(targetPath, entry));
        }
    }

    private void copySingleAsset(String assetPath, File targetPath) throws IOException {
        final File parent = targetPath.getParentFile();
        if(parent != null && !parent.exists() && !parent.mkdirs() && !parent.isDirectory()) {
            throw new IOException("Could not create asset parent directory " + parent.getAbsolutePath());
        }

        try (InputStream inputStream = getAssets().open(assetPath);
             OutputStream outputStream = new FileOutputStream(targetPath, false)) {
            final byte[] buffer = new byte[64 * 1024];
            int read;
            while((read = inputStream.read(buffer)) != -1) {
                outputStream.write(buffer, 0, read);
            }
        }
    }

    private void applyImmersiveFullscreen() {
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN);
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
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
