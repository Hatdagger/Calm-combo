package com.hatdagger.calmcombo;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.Rect;
import android.view.Choreographer;
import android.view.MotionEvent;
import android.view.View;

import java.nio.ByteBuffer;

/**
 * The game screen.  All the game runs in C (android/core); this view feeds it
 * time, touches and keys, and shows the small picture it draws, scaled up by
 * a whole number so the pixel art stays sharp.
 */
public class GameView extends View implements Choreographer.FrameCallback {
    static {
        System.loadLibrary("calm");
    }

    static native int nativeInit(ByteBuffer pak, byte[] save);
    static native int nativePickScale(int w, int h);
    static native void nativeResize(int w, int h);
    static native void nativeTick(int n);
    static native void nativeRender(Bitmap bmp);
    static native void nativeTouch(int action, int x, int y);
    static native void nativeKey(int key, boolean down);
    static native int nativePopSound();
    static native int nativeMusic();
    static native boolean nativeQuit();
    static native byte[] nativeSave();
    static native void nativeMedia(int flags, String title);
    static native int nativePopMedia();
    static native boolean nativeBarFits(int w, int h);

    static final int CT_DOWN = 0, CT_MOVE = 1, CT_UP = 2, CT_CANCEL = 3;
    static final long TICK_NS = 1_000_000_000L / 60;

    interface Host {
        void playSound(int id);
        void setMusic(int track);
        void store(byte[] save);
        void quit();
        void mediaAction(int action);
    }

    private final Host host;
    private final Paint paint = new Paint();
    private final Rect dst = new Rect();
    private Bitmap bmp;
    private int scale = 1;
    private long last = 0, acc = 0;
    private boolean running = false;
    private int music = -1;

    public GameView(Context ctx, Host host) {
        super(ctx);
        this.host = host;
        paint.setFilterBitmap(false);
        paint.setAntiAlias(false);
        setFocusable(true);
        setFocusableInTouchMode(true);
        setKeepScreenOn(true);
    }

    @Override
    protected void onSizeChanged(int w, int h, int ow, int oh) {
        if (w <= 0 || h <= 0) return;
        scale = pickScale(w, h);
        int lw = Math.max(160, Math.min(400, w / scale));
        int lh = Math.max(280, Math.min(720, h / scale));
        bmp = Bitmap.createBitmap(lw, lh, Bitmap.Config.ARGB_8888);
        nativeResize(lw, lh);
        int dw = lw * scale, dh = lh * scale;
        dst.set((w - dw) / 2, (h - dh) / 2, (w - dw) / 2 + dw, (h - dh) / 2 + dh);
        nativeRender(bmp);
    }

    /*
     * The biggest whole-number zoom, but on big sharp screens (tablets) step
     * down a little if that makes room for the music bar, as long as a tile
     * stays a comfortable size to touch (about 5.5 mm).
     */
    private int pickScale(int w, int h) {
        int best = nativePickScale(w, h);
        if (nativeBarFits(clampW(w / best), clampH(h / best))) return best;
        android.util.DisplayMetrics dm = getResources().getDisplayMetrics();
        float dpi = dm.xdpi > 100 && dm.xdpi < 1000 ? dm.xdpi : dm.densityDpi;
        for (int s = best - 1; s >= 1; s--) {
            if (16f * s / dpi * 25.4f < 5.5f) break;
            if (w / s > 400 || h / s > 720) break;
            if (nativeBarFits(clampW(w / s), clampH(h / s))) return s;
        }
        return best;
    }

    private static int clampW(int lw) { return Math.max(160, Math.min(400, lw)); }
    private static int clampH(int lh) { return Math.max(280, Math.min(720, lh)); }

    void start() {
        if (running) return;
        running = true;
        last = 0;
        Choreographer.getInstance().postFrameCallback(this);
    }

    void stop() {
        running = false;
        Choreographer.getInstance().removeFrameCallback(this);
    }

    @Override
    public void doFrame(long now) {
        if (!running) return;
        Choreographer.getInstance().postFrameCallback(this);
        if (last == 0) last = now;
        acc += now - last;
        last = now;
        int n = (int) (acc / TICK_NS);
        acc -= n * TICK_NS;
        if (n > 4) { n = 4; acc = 0; }           /* after a stall, don't race to catch up */
        if (n == 0) return;
        nativeTick(n);
        if (bmp != null) {
            nativeRender(bmp);
            invalidate();
        }
        int s;
        while ((s = nativePopSound()) >= 0) host.playSound(s);
        while ((s = nativePopMedia()) >= 0) host.mediaAction(s);
        int m = nativeMusic();
        if (m != music) { music = m; host.setMusic(m); }
        byte[] save = nativeSave();
        if (save != null) host.store(save);
        if (nativeQuit()) host.quit();
    }

    @Override
    protected void onDraw(Canvas canvas) {
        canvas.drawColor(Color.BLACK);
        if (bmp != null) canvas.drawBitmap(bmp, null, dst, paint);
    }

    @Override
    public boolean onTouchEvent(MotionEvent e) {
        int a;
        switch (e.getActionMasked()) {
            case MotionEvent.ACTION_DOWN: a = CT_DOWN; break;
            case MotionEvent.ACTION_MOVE: a = CT_MOVE; break;
            case MotionEvent.ACTION_UP: a = CT_UP; break;
            case MotionEvent.ACTION_CANCEL: a = CT_CANCEL; break;
            default: return true;                 /* only the first finger plays */
        }
        int x = (int) Math.floor((e.getX(0) - dst.left) / scale);
        int y = (int) Math.floor((e.getY(0) - dst.top) / scale);
        nativeTouch(a, x, y);
        return true;
    }

    void forceMusic() { music = -1; }
}
