/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */
package org.mini.gui;

import org.mini.glfw.Glfw;
import org.mini.gui.event.GActionListener;
import org.mini.gui.event.GFocusChangeListener;
import org.mini.nanovg.Nanovg;
import org.mini.reflect.ReflectArray;

import java.io.FileInputStream;
import java.io.InputStream;
import java.util.*;

import static org.mini.glwrap.GLUtil.toUtf8;
import static org.mini.nanovg.Nanovg.*;

/**
 * @author gust
 */
public class GToolkit {

    static Hashtable<Long, GForm> table = new Hashtable();

    static public GForm getForm(long ctx) {
        return table.get(ctx);
    }

    static public GForm removeForm(long ctx) {
        return table.remove(ctx);
    }

    static public void putForm(long ctx, GForm win) {
        table.put(ctx, win);
    }

    /**
     * 返回数组数据区首地址
     *
     * @param array
     * @return
     */
    public static long getArrayDataPtr(Object array) {
        return ReflectArray.getBodyPtr(array);
    }

    public static float[] nvgRGBA(int r, int g, int b, int a) {
        return Nanovg.nvgRGBA((byte) r, (byte) g, (byte) b, (byte) a);
    }

    public static float[] nvgRGBA(int rgba) {
        return Nanovg.nvgRGBA((byte) ((rgba >> 24) & 0xff), (byte) ((rgba >> 16) & 0xff), (byte) ((rgba >> 8) & 0xff), (byte) ((rgba >> 0) & 0xff));
    }

    public static byte[] readFileFromJar(String path) {
        try {

            InputStream is = GCallBack.getInstance().getResourceAsStream(path);
            if (is != null) {
                int av = is.available();

                if (av >= 0) {
                    byte[] b = new byte[av];
                    int r, read = 0;
                    while (read < av) {
                        r = is.read(b, read, av - read);
                        read += r;
                    }
                    return b;
                }
            }
        } catch (Exception ex) {
            ex.printStackTrace();
        }
        System.out.println("load from jar fail : " + path);
        return null;
    }

    public static String readFileFromJarAsString(String path, String encode) {
        try {
            byte[] cont = readFileFromJar(path);
            String s = new String(cont, encode);
            return s;
        } catch (Exception ex) {
            ex.printStackTrace();
        }
        return null;
    }

    public static byte[] readFileFromFile(String path) {
        try {

            InputStream is = new FileInputStream(path);
            if (is != null) {
                int av = is.available();

                if (av >= 0) {
                    byte[] b = new byte[av];
                    int r, read = 0;
                    while (read < av) {
                        r = is.read(b, read, av - read);
                        read += r;
                    }
                    return b;
                }
            }
        } catch (Exception ex) {
            ex.printStackTrace();
        }
        System.out.println("load from file fail : " + path);
        return null;
    }

    public static String readFileFromFileAsString(String path, String encode) {
        try {
            byte[] cont = readFileFromFile(path);
            String s = new String(cont, encode);
            return s;
        } catch (Exception ex) {
            ex.printStackTrace();
        }
        return null;
    }
    /**
     * ----------------------------------------------------------------
     *      font
     * ----------------------------------------------------------------
     */

    /**
     * 字体部分
     */
    static byte[] FONT_GLYPH_TEMPLATE = toUtf8("正");

    public static class FontHolder {

        static byte[] font_word = toUtf8("word"), font_icon = toUtf8("icon");
        static int font_word_handle, font_icon_handle, font_emoji_handle;
        static boolean fontLoaded = false;
        static byte[] data_word;
        static byte[] data_icon;

        public static synchronized void loadFont(long vg) {
            if (fontLoaded) {
                return;
            }
            data_word = readFileFromJar("/res/NotoEmoji+NotoSansCJKSC-Regular.ttf");
//            data_word = readFileFromJar("/res/out.ttf");
            font_word_handle = Nanovg.nvgCreateFontMem(vg, font_word, data_word, data_word.length, 0);
            if (font_word_handle == -1) {
                System.out.println("Could not add font.\n");
            }
            nvgAddFallbackFontId(vg, font_word_handle, font_word_handle);

            data_icon = readFileFromJar("/res/entypo.ttf");
            font_icon_handle = Nanovg.nvgCreateFontMem(vg, font_icon, data_icon, data_icon.length, 0);
            if (font_icon_handle == -1) {
                System.out.println("Could not add font.\n");
            }

            fontLoaded = true;
        }
    }

    public static byte[] getFontWord() {
        return FontHolder.font_word;
    }

    public static byte[] getFontIcon() {
        return FontHolder.font_icon;
    }

    public static float[] getFontBoundle(long vg) {
        float[] bond = new float[4];
        nvgTextBoundsJni(vg, 0f, 0f, FONT_GLYPH_TEMPLATE, 0, FONT_GLYPH_TEMPLATE.length, bond);
        bond[GObject.WIDTH] -= bond[GObject.LEFT];
        bond[GObject.HEIGHT] -= bond[GObject.TOP];
        bond[GObject.LEFT] = bond[GObject.TOP] = 0;
        return bond;
    }

    public static float[] getFontBoundle(long vg, byte[] fontName, float fontSize) {
        float[] bond = new float[4];
        nvgSave(vg);
        nvgFontSize(vg, fontSize);
        nvgFontFace(vg, fontName);
        nvgTextBoundsJni(vg, 0f, 0f, FONT_GLYPH_TEMPLATE, 0, FONT_GLYPH_TEMPLATE.length, bond);
        bond[GObject.WIDTH] -= bond[GObject.LEFT];
        bond[GObject.HEIGHT] -= bond[GObject.TOP];
        bond[GObject.LEFT] = bond[GObject.TOP] = 0;
        nvgRestore(vg);
        return bond;
    }

    public static byte[] getDefaultFont() {
        return FontHolder.font_word;
    }


    /**
     * ----------------------------------------------------------------
     *      style
     * ----------------------------------------------------------------
     */

    /**
     * 风格
     */
    static GStyleInner defaultStyle;

    public static GStyle getStyle() {
        if (defaultStyle == null) {
            defaultStyle = new GStyleInner(new GStyleBright());
        }
        return defaultStyle;
    }

    public static void setStyle(GStyle style) {
        if (style == null) return;
        defaultStyle = new GStyleInner(style);//copy every times
    }

    /**
     * ----------------------------------------------------------------
     *      draw
     * ----------------------------------------------------------------
     */

    /**
     * 光标
     */
    static boolean caretBlink = false;
    static long caretLastBlink;
    static long CARET_BLINK_PERIOD = 600;

    /**
     * 画光标，是否闪烁，如果为false,则一常显，为了节能，所以大多时候blink为false
     *
     * @param vg
     * @param x
     * @param y
     * @param w
     * @param h
     * @param blink
     */
    public static void drawCaret(long vg, float x, float y, float w, float h, boolean blink) {
        long curTime = System.currentTimeMillis();
        if (curTime - caretLastBlink > CARET_BLINK_PERIOD) {
            caretBlink = !caretBlink;
            caretLastBlink = curTime;
        }
        if (caretBlink || !blink) {
            nvgBeginPath(vg);
            nvgFillColor(vg, nvgRGBA(255, 192, 0, 255));
            nvgRect(vg, x, y, w, h);
            nvgFill(vg);
        }
    }

    static float[] RED_POINT_BACKGROUND = Nanovg.nvgRGBAf(1.f, 0, 0, 1.f);
    static float[] RED_POINT_FRONT = Nanovg.nvgRGBAf(1.f, 1.f, 1.f, 1.f);

    public static void drawRedPoint(long vg, String text, float x, float y, float r) {
        nvgBeginPath(vg);
        nvgCircle(vg, x, y, r);
        nvgFillColor(vg, RED_POINT_BACKGROUND);
        nvgFill(vg);

        nvgFontSize(vg, r * 2 - 4);
        nvgFillColor(vg, RED_POINT_FRONT);
        nvgFontFace(vg, GToolkit.getFontWord());
        byte[] text_arr = toUtf8(text);
        nvgTextAlign(vg, Nanovg.NVG_ALIGN_CENTER | Nanovg.NVG_ALIGN_MIDDLE);
        if (text_arr != null) {
            Nanovg.nvgTextJni(vg, x, y + 1, text_arr, 0, text_arr.length);
        }

    }

    public static void drawCircle(long vg, float x, float y, float r, float[] color, boolean fill) {
        nvgBeginPath(vg);
        nvgCircle(vg, x, y, r);
        if (fill) {
            nvgFillColor(vg, color);
            nvgFill(vg);
        } else {
            nvgStrokeColor(vg, color);
            nvgStrokeWidth(vg, 1.0f);
            nvgStroke(vg);
        }
    }

    public static void drawRect(long vg, float x, float y, float w, float h, float[] color) {
        drawRect(vg, x, y, w, h, color, true);
    }

    public static void drawRect(long vg, float x, float y, float w, float h, float[] color, boolean fill) {
        nvgBeginPath(vg);
        nvgRect(vg, x, y, w, h);
        if (fill) {
            nvgFillColor(vg, color);
            nvgFill(vg);
        } else {
            nvgStrokeColor(vg, color);
            nvgStrokeWidth(vg, 1.0f);
            nvgStroke(vg);
        }
    }

    public static void drawRoundedRect(long vg, float x, float y, float w, float h, float r, float[] color) {
        nvgBeginPath(vg);
        nvgFillColor(vg, color);
        nvgRoundedRect(vg, x, y, w, h, r);
        nvgFill(vg);
    }

    public static float[] getTextBoundle(long vg, String s, float width) {
        return getTextBoundle(vg, s, width, GToolkit.getStyle().getTextFontSize(), GToolkit.getFontWord());
    }

    public static float[] getTextBoundle(long vg, String s, float width, float fontSize) {
        return getTextBoundle(vg, s, width, fontSize, GToolkit.getFontWord());
    }

    public static float[] getTextBoundle(long vg, String s, float width, float fontSize, byte[] font) {
        float[] bond = new float[4];
        byte[] b = toUtf8(s);
        nvgFontSize(vg, fontSize);
        nvgFontFace(vg, font);
        nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
        nvgTextBoxBoundsJni(vg, 0, 0, width - GLabel.TEXT_BOUND_DEC, b, 0, b.length, bond);
        bond[GObject.WIDTH] -= bond[GObject.LEFT];
        bond[GObject.HEIGHT] -= bond[GObject.TOP];
        bond[GObject.LEFT] = bond[GObject.TOP] = 0;
        bond[GObject.WIDTH] += GLabel.TEXT_BOUND_DEC;
        return bond;
    }

    public static void drawTextLine(long vg, float tx, float ty, String s, float fontSize, float[] color, int align) {
        drawTextLineWithShadow(vg, tx, ty, s, fontSize, color, align, null, 0);
    }

    public static void drawTextLineWithShadow(long vg, float tx, float ty, String s, float fontSize, float[] color, int align, float[] shadowColor, float shadowBlur) {
        if (s == null) {
            return;
        }
        nvgFontSize(vg, fontSize);
        nvgFontFace(vg, GToolkit.getFontWord());
        nvgTextAlign(vg, align);
        byte[] b = toUtf8(s);
        if (shadowColor != null) {
            nvgFontBlur(vg, shadowBlur);
            nvgFillColor(vg, color);
            Nanovg.nvgTextJni(vg, tx, ty + 1.5f, b, 0, b.length);
            nvgFontBlur(vg, 0);
        }
        nvgFillColor(vg, color);
        Nanovg.nvgTextJni(vg, tx, ty + 1.5f, b, 0, b.length);
    }

    public static void drawTextLineInBoundle(long vg, float tx, float ty, float pw, float ph, String s, float fontSize, float[] color) {
        if (s == null) {
            return;
        }
        nvgSave(vg);
        nvgScissor(vg, tx, ty, pw, ph);
        nvgFontSize(vg, fontSize);
        nvgFontFace(vg, GToolkit.getFontWord());
        nvgTextAlign(vg, NVG_ALIGN_TOP | NVG_ALIGN_LEFT);
        nvgFillColor(vg, color);
        byte[] b = toUtf8(s);
        Nanovg.nvgTextJni(vg, tx, ty + 1.5f, b, 0, b.length);
        nvgRestore(vg);
    }


    public static void drawText(long vg, float x, float y, float w, float h, String s) {

        drawTextWithShadow(vg, x, y, w, h, s, GToolkit.getStyle().getTextFontSize(), GToolkit.getStyle().getTextFontColor(), null, 0);
    }

    public static void drawText(long vg, float x, float y, float w, float h, String s, float fontSize, float[] color) {
        drawTextWithShadow(vg, x, y, w, h, s, fontSize, color, null, 0);
    }

    public static void drawTextWithShadow(long vg, float x, float y, float w, float h, String s, float fontSize, float[] color, float[] shadowColor, float shadowBlur) {
        if (s == null) {
            return;
        }
        nvgFontSize(vg, fontSize);
        nvgFontFace(vg, GToolkit.getFontWord());

        byte[] text_arr = toUtf8(s);

        nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);

        if (text_arr != null) {
            if (shadowColor != null) {
                nvgFontBlur(vg, shadowBlur);
                nvgFillColor(vg, shadowColor);
                nvgTextBoxJni(vg, x + 1, y + 2, w, text_arr, 0, text_arr.length);
                nvgFontBlur(vg, 0);
            }
            nvgFillColor(vg, color);
            nvgTextBoxJni(vg, x, y + 1, w, text_arr, 0, text_arr.length);
        }
    }

    public static void drawImageFrame(long vg, GImage img, int imgCols, int imgRows, int frameIndex, float px, float py, float pw, float ph) {
        drawImageFrame(vg, img, imgCols, imgRows, frameIndex, px, py, pw, ph, false, 0, 1.0f);
    }

    /**
     * 用于画很多行列帧组成的图片中的一帧
     *
     * @param vg
     * @param img
     * @param imgCols    图片有多少列
     * @param imgRows    图片有多少行
     * @param frameIndex 画第几个图片
     * @param px
     * @param py
     * @param pw
     * @param ph
     * @param border
     * @param alpha
     */
    public static void drawImageFrame(long vg, GImage img, int imgCols, int imgRows, int frameIndex, float px, float py, float pw, float ph, boolean border, float radius, float alpha) {
        if (img == null) {
            return;
        }
        Nanovg.nvgSave(vg);
        if (radius < 0) radius = 0;
        int frameCol = frameIndex % imgCols;
        int frameRow = frameIndex / imgCols;

        float drawX = px - pw * frameCol;
        float drawY = py - ph * frameRow;
        float drawW = pw * imgCols;
        float drawH = ph * imgRows;
        Nanovg.nvgScissor(vg, px, py, pw, ph);

        if (border) {
            drawX += 1;
            drawY += 1;
            drawW -= 2;
            drawH -= 2;
        }
        byte[] imgPaint = nvgImagePattern(vg, drawX, drawY, drawW, drawH, 0.0f / 180.0f * (float) Math.PI, img.getNvgTextureId(vg), alpha);
        nvgBeginPath(vg);
        nvgRoundedRect(vg, px, py, pw, ph, radius + (border ? 1 : 0));
        nvgFillPaint(vg, imgPaint);
        nvgFill(vg);

        if (border) {
            nvgBeginPath(vg);
            nvgRoundedRect(vg, px, py, pw, ph, radius);
            nvgStrokeWidth(vg, 1.0f);
            nvgStrokeColor(vg, nvgRGBA(255, 255, 255, 192));
            nvgStroke(vg);
        }
        Nanovg.nvgRestore(vg);
    }

    /**
     * 画图
     *
     * @param vg
     * @param img
     * @param px
     * @param py
     * @param pw
     * @param ph
     */
    public static void drawImage(long vg, GImage img, float px, float py, float pw, float ph) {
        drawImage(vg, img, px, py, pw, ph, true, 1.f);
    }

    public static void drawImage(long vg, GImage img, float px, float py, float pw, float ph, boolean border, float alpha) {
        if (img == null) {
            return;
        }

        byte[] shadowPaint, imgPaint;
        float ix, iy, iw, ih;
        int[] imgW = {0}, imgH = {0};
        imgW[0] = img.getWidth();
        imgH[0] = img.getHeight();

        //nvgImageSize(vg, img.getTexture(vg), imgW, imgH);
        if (imgW[0] < imgH[0]) {
            iw = pw;
            ih = iw * (float) imgH[0] / (float) imgW[0];
            ix = 0;
            iy = -(ih - ph) * 0.5f;
        } else {
            ih = ph;
            iw = ih * (float) imgW[0] / (float) imgH[0];
            ix = -(iw - pw) * 0.5f;
            iy = 0;
        }

        imgPaint = nvgImagePattern(vg, px + ix + 1, py + iy + 1, iw - 2, ih - 2, 0.0f / 180.0f * (float) Math.PI, img.getNvgTextureId(vg), alpha);
        nvgBeginPath(vg);
        nvgRoundedRect(vg, px, py, pw, ph, border ? 5f : 0f);
        nvgFillPaint(vg, imgPaint);
        nvgFill(vg);

        if (border) {
//            shadowPaint = nvgBoxGradient(vg, px, py, pw, ph, 5, 3, nvgRGBA(0, 0, 0, 128), nvgRGBA(0, 0, 0, 0));
//            nvgBeginPath(vg);
//            //nvgRect(vg, px - 5, py - 5, pw + 10, ph + 10);
//            nvgRoundedRect(vg, px, py, pw, ph, 6);
//            nvgPathWinding(vg, NVG_HOLE);
//            nvgFillPaint(vg, shadowPaint);
//            nvgFill(vg);

            nvgBeginPath(vg);
            nvgRoundedRect(vg, px + 1, py + 1, pw - 2, ph - 2, border ? 3.5f : 0f);
            nvgStrokeWidth(vg, 1.0f);
            nvgStrokeColor(vg, nvgRGBA(255, 255, 255, 192));
            nvgStroke(vg);
        }
    }
    /**
     * ----------------------------------------------------------------
     *      frame
     * ----------------------------------------------------------------
     */

    /**
     * return a frame to confirm msg
     *
     * @param title
     * @param msg
     * @param left
     * @param leftListener
     * @param right
     * @param rightListener
     * @return
     */
    static public GFrame getConfirmFrame(GForm form, String title, String msg, String left, GActionListener leftListener, String right, GActionListener rightListener) {
        return getConfirmFrame(form, title, msg, left, leftListener, right, rightListener, 300, form.getH() * .6f);
    }

    static public GFrame getConfirmFrame(GForm form, String title, String msg, String left, GActionListener leftListener, String right, GActionListener rightListener, float width, float height) {
        GFrame frame = new GFrame(form, title, 0, 0, width, height);
        frame.setFront(true);
        frame.setFocusListener(new GFocusChangeListener() {
            @Override
            public void focusGot(GObject go) {
            }

            @Override
            public void focusLost(GObject go) {
                if (frame.getForm() != null) {
                    frame.getForm().remove(frame);
                }
            }
        });

        GContainer gp = frame.getView();
        float x = 10, y = 5, w = gp.getW() - 20, h = gp.getH() - 50;

        GTextBox tbox = new GTextBox(form, msg, "", x, y, w, h);
        tbox.setEditable(false);
        gp.add(tbox);
        y += h + 5;

        float btnWidth = w * .5f;
        if (left != null) {
            GButton leftBtn = new GButton(form, left, x, y, btnWidth, 30);
            //leftBtn.setBgColor(128, 16, 8, 255);
            gp.add(leftBtn);
            leftBtn.setActionListener(leftListener);
        }

        GButton rightBtn = new GButton(form, right == null ? GLanguage.getString("Cancel") : right, btnWidth + 10, y, btnWidth, 30);
        gp.add(rightBtn);
        rightBtn.setActionListener(rightListener == null ? (GActionListener) gobj -> frame.close() : rightListener);

        return frame;
    }

    static public GFrame getMsgFrame(GForm form, String title, String msg) {
        return getMsgFrame(form, title, msg, 300, form.getH() * .6f);
    }

    static public GFrame getMsgFrame(GForm form, String title, String msg, float width, float height) {
        final GFrame frame = new GFrame(form, title, 0, 0, width, height);
        frame.setFront(true);
        frame.setFocusListener(new GFocusChangeListener() {
            @Override
            public void focusGot(GObject go) {
            }

            @Override
            public void focusLost(GObject go) {
                if (frame.getForm() != null) {
                    frame.getForm().remove(frame);
                }
            }
        });

        GContainer gp = frame.getView();
        float x = 10, y = 10, w = gp.getW() - 20, h = gp.getH() - 50;

        GTextBox tbox = new GTextBox(form, msg, "", x, y, w, h);
        tbox.setEditable(false);
        gp.add(tbox);
        y += h + 10;

        float btnWidth = w * .5f;
        GButton leftBtn = new GButton(form, GLanguage.getString("Ok"), x + btnWidth * .5f, y, btnWidth, 28);
        //leftBtn.setBgColor(128, 16, 8, 255);
        leftBtn.setName("MSG_FRAME_OK");
        gp.add(leftBtn);
        leftBtn.setActionListener(gobj -> frame.close());

        return frame;
    }

    /**
     * @param title
     * @param strs
     * @param imgs
     * @param buttonListener
     * @param itemListener
     * @return
     */
    public static GFrame getListFrame(GForm form, String title, String[] strs, GImage[] imgs, GActionListener buttonListener, GActionListener itemListener) {
        return getListFrame(form, title, strs, imgs, false, null, buttonListener, itemListener);
    }

    public static GFrame getListFrame(GForm form, String title, String[] strs, GImage[] imgs, boolean multiSelect, String buttonText, GActionListener buttonListener, GActionListener itemListener) {
        return getListFrame(form, title, strs, imgs, multiSelect, buttonText, buttonListener, itemListener, 300, 250);
    }

    public static GFrame getListFrame(GForm form, String title, String[] strs, GImage[] imgs, boolean multiSelect, String buttonText, GActionListener buttonListener, GActionListener itemListener, float width, float height) {
        float pad = 2, btnW, btnH = 28;
        float y = pad;

        GFrame frame = new GFrame(form, title, 0, 0, width, height);

        frame.setFront(true);
        frame.setFocusListener(new GFocusChangeListener() {
            @Override
            public void focusGot(GObject go) {
            }

            @Override
            public void focusLost(GObject go) {
                frame.close();
            }
        });
        GContainer view = frame.getView();

        GTextField search = new GTextField(form, "", "search", pad, y, view.getW() - pad * 2, 30);
        search.setName("search");
        search.setBoxStyle(GTextField.BOX_STYLE_SEARCH);

        view.add(search);
        y += 30 + pad;

        float h = view.getH() - y - 30 - pad * 4;
        GList glist = new GList(form, 0, y, view.getW(), h);
        glist.setName("list");
        glist.setShowMode(GList.MODE_MULTI_SHOW);
        glist.setSelectMode(multiSelect ? GList.MODE_MULTI_SELECT : GList.MODE_SINGLE_SELECT);

        search.setStateChangeListener(gobj -> {
            GTextObject so = (GTextObject) gobj;
            String str = so.getText();
            if (glist != null) {
                glist.filterLabelWithKey(str);
                //System.out.println("key=" + str);
            }
        });

        view.add(glist);
        y += h + pad;
        btnW = view.getW() * .5f - pad;
        if (multiSelect) {
            GCheckBox chbox = new GCheckBox(form, GLanguage.getString("SeleAll"), false, pad, y, btnW, btnH);
            view.add(chbox);
            chbox.setActionListener(gobj -> {
                if (((GCheckBox) gobj).isChecked()) {
                    glist.selectAll();
                } else {
                    glist.deSelectAll();
                }
            });
        }


        GButton btn = new GButton(form, buttonText == null ? GLanguage.getString("Ok") : buttonText, (view.getW() - btnW - pad), y, btnW, btnH);
        btn.setName("perform");
        frame.getView().add(btn);
        btn.setActionListener(buttonListener);
        //
        glist.setItems(imgs, strs);
        if (itemListener != null) {
            for (GListItem item : glist.getItems()) {
                item.setActionListener(itemListener);
            }
        }
        return frame;
    }

    public static GFrame getInputFrame(GForm form, String title, String msg, String defaultValue, String inputHint, String leftLabel, GActionListener leftListener, String rightLabel, GActionListener rightListener) {
        return getInputFrame(form, title, msg, defaultValue, inputHint, leftLabel, leftListener, rightLabel, rightListener, 300, 200);
    }

    public static GFrame getInputFrame(GForm form, String title, String msg, String defaultValue, String inputHint, String leftLabel, GActionListener leftListener, String rightLabel, GActionListener rightListener, float width, float height) {

        float x = 10, y;
        GFrame frame = new GFrame(form, title, 0, 0, width, height);
        frame.setFront(true);
        frame.setFocusListener(new GFocusChangeListener() {
            @Override
            public void focusGot(GObject oldgo) {
            }

            @Override
            public void focusLost(GObject newgo) {
                frame.close();
            }
        });
        GContainer view = frame.getView();
        float contentWidth;
        contentWidth = view.getW() - 20;
        y = view.getH();

        float buttonWidth = contentWidth * .5f - 10;
        y -= 35f;
        GButton cancelbtn = new GButton(form, leftLabel == null ? GLanguage.getString("Cancel") : leftLabel, x, y, buttonWidth, 28);
        view.add(cancelbtn);

        GButton okbtn = new GButton(form, rightLabel == null ? GLanguage.getString("Ok") : rightLabel, x + buttonWidth + 20, y, buttonWidth, 28);
        //okbtn.setBgColor(0, 96, 128, 255);
        view.add(okbtn);
        y -= 35;
        GTextField input = new GTextField(form, defaultValue == null ? "" : defaultValue, inputHint, x, y, contentWidth, 28);
        input.setName("input");
        view.add(input);

        y -= 25;
        GLabel lb_state = new GLabel(form, "", x, y, contentWidth, 20);
        lb_state.setName("state");
        view.add(lb_state);

        y = 10;
        GLabel lb1 = new GLabel(form, msg, x, y, contentWidth, y);
        lb1.setShowMode(GLabel.MODE_MULTI_SHOW);
        view.add(lb1);

        if (rightListener != null) {
            okbtn.setActionListener(rightListener);
        } else {
            okbtn.setActionListener((GObject gobj) -> {
                if (gobj.getFrame() != null) {
                    gobj.getFrame().close();
                }
            });
        }

        if (leftListener != null) {
            cancelbtn.setActionListener(leftListener);
        } else {
            cancelbtn.setActionListener((GObject gobj) -> {
                if (gobj.getFrame() != null) {
                    gobj.getFrame().close();
                }
            });
        }
        return frame;
    }

    public static GList getListMenu(GForm form, String[] strs, GImage[] imgs, GActionListener[] listeners) {
        return getListMenu(form, strs, imgs, listeners, 150, 120);
    }

    public static GList getListMenu(GForm form, String[] strs, GImage[] imgs, GActionListener[] listeners, float width, float height) {

        GList list = new GList(form, 0, 0, width, height);
        list.setBgColor(getStyle().getPopBackgroundColor());
        list.setShowMode(GList.MODE_MULTI_SHOW);
        list.setFocusListener(new GFocusChangeListener() {
            @Override
            public void focusGot(GObject oldgo) {
            }

            @Override
            public void focusLost(GObject newgo) {
                if (list.getForm() != null) {
                    list.getForm().remove(list);
                }
            }
        });

        GActionListener common = new GActionListener() {
            GActionListener[] actions = listeners;

            @Override
            public void action(GObject gobj) {
                list.getParent().remove(list);
                int i = list.getSelectedIndex();
                if (actions != null && actions.length > i) {
                    actions[i].action(gobj);
                }
            }
        };

        list.setItems(imgs, strs);
        GListItem[] items = list.getItems();
        if (listeners != null) {
            for (int i = 0, imax = items.length; i < imax; i++) {
                items[i].setActionListener(common);
            }
        }
        list.setSize(width, height);
//        int size = items.length;
//        if (size > 8) {
//            size = 8;
//        }
//        list.setInnerSize(200, size * list.list_item_heigh);
        list.setFront(true);
        list.setName("listmenu");

        return list;
    }

    public static GMenu getMenu(GForm form, String[] strs, GImage[] imgs, GActionListener[] listener) {

        GMenu menu = new GMenu(form, 0, 0, 150, 120);
        menu.setFocusListener(new GFocusChangeListener() {
            @Override
            public void focusGot(GObject oldgo) {
            }

            @Override
            public void focusLost(GObject newgo) {
                if (menu.getForm() != null) {
                    menu.getForm().remove(menu);
                }
            }
        });

        for (int i = 0, imax = strs.length; i < imax; i++) {
            GMenuItem item = menu.addItem(strs[i], imgs == null ? null : imgs[i]);
            if (listener != null) {
                item.setActionListener(listener[i]);
            }
        }

        int size = strs.length;
        if (size > 5) {
            size = 5;
        }
        menu.setSize(300, 40);
        menu.setFront(true);

        return menu;
    }

    public static GViewPort getImageView(GForm form, GImage img, GActionListener listener) {

        GViewPort view = new GViewPort(form) {
            GImage image = img;

            @Override
            public void longTouchedEvent(int x, int y) {
                GList menu = new GList(form);
                GListItem item = menu.addItem(null, GLanguage.getString("Save to album"));
                item.setActionListener((GObject gobj) -> {
                });
                item = menu.addItem(null, GLanguage.getString("Cancel"));
                item.setActionListener((GObject gobj) -> {
                    if (gobj.getForm() != null) {
                        gobj.getForm().remove(menu);
                    }
                });
                add(menu);
            }

            @Override
            public boolean paint(long vg) {
                float w = getW();
                float h = getH();

                GToolkit.drawImage(vg, image, getX(), getY(), w, h);

                return true;
            }

            @Override
            public void touchEvent(int touchid, int phase, int x, int y) {
                if (touchid != Glfw.GLFW_MOUSE_BUTTON_1) return;
                if (listener != null) {
                    listener.action(this);
                } else {
                    if (getForm() != null) {
                        List<GObject> list = getElements();
                        synchronized (list) {
                            if (getElements().isEmpty()) {//no menu
                                getForm().remove(this);
                                //System.out.println("picture removed");
                            }
                        }
                    }
                }
            }
        };
        view.setFocusListener(new GFocusChangeListener() {
            @Override
            public void focusGot(GObject oldgo) {
            }

            @Override
            public void focusLost(GObject newgo) {
                if (view.getForm() != null) {
                    view.getForm().remove(view);
                }
            }
        });

        float imgW = img.getWidth();
        float imgH = img.getHeight();

        float formW = form.getW();
        float formH = form.getH();

        float ratioW = formW / imgW;
        float ratioH = formH / imgH;

        if (formW < formH) {
            imgW *= ratioW;
            imgH *= ratioW;
        } else {
            imgW *= ratioH;
            imgH *= ratioH;
        }
        view.setSize(imgW, imgH);
        view.setLocation((formW - view.getW()) / 2, (formH - view.getH()) / 2);

        return view;
    }

    public static void showFrame(GObject gobj) {
        if (gobj == null) return;
        GForm form = gobj.getForm();
        gobj.setLocation(form.getW() / 2 - gobj.getW() / 2, form.getH() / 2 - gobj.getH() / 2);
        form.add(gobj);
        form.setFocus(gobj);
    }

    public static void showFrame(GObject gobj, float x, float y) {
        if (gobj == null) return;
        GForm form = gobj.getForm();
        gobj.setLocation(x, y);
        form.add(gobj);
        form.setFocus(gobj);
    }

    public static void closeFrame(GForm form, String frameName) {
        if (frameName == null) return;
        GObject go = form.findByName(frameName);
        if (go != null) {
            form.remove(go);
        }
    }

    public static void showAlignedFrame(GObject gobj, int align_mod) {
        if (gobj == null) return;
        GForm form = gobj.getForm();
        if (form == null) {
            System.out.println("warning: added to form can be set align");
            return;
        }
        if ((align_mod & Nanovg.NVG_ALIGN_LEFT) != 0) {
            gobj.setLocation(0, gobj.getY());
        } else if ((align_mod & Nanovg.NVG_ALIGN_RIGHT) != 0) {
            gobj.setLocation(form.getW() - gobj.getW(), gobj.getY());
        } else if ((align_mod & Nanovg.NVG_ALIGN_CENTER) != 0) {
            gobj.setLocation((form.getW() - gobj.getW()) * .5f, gobj.getY());
        }
        if ((align_mod & Nanovg.NVG_ALIGN_TOP) != 0) {
            gobj.setLocation(gobj.getX(), 0);
        } else if ((align_mod & Nanovg.NVG_ALIGN_BOTTOM) != 0) {
            gobj.setLocation(gobj.getX(), form.getH() - gobj.getH());
        } else if ((align_mod & Nanovg.NVG_ALIGN_CENTER) != 0) {
            gobj.setLocation(gobj.getX(), (form.getH() - gobj.getH()) * .5f);
        }
        form.add(gobj);
    }


    /**
     * set component attachment by compName
     * find component from parent
     *
     * @param parent
     * @param compName
     * @param o
     */
    public static void setCompAttachment(GContainer parent, String compName, Object o) {
        if (compName == null || o == null || parent == null) return;
        GObject go = parent.findByName(compName);
        if (go != null) {
            go.setAttachment(o);
        }
    }

    public static <T extends Object> T getCompAttachment(GContainer parent, String compName) {
        if (compName != null || parent == null) {
            GObject go = parent.findByName(compName);
            if (go != null) {
                return go.getAttachment();
            }
        }
        return null;
    }

    public static Boolean getCompEnable(GContainer parent, String compName) {
        if (compName == null || parent == null) return false;
        GObject eitem = parent.findByName(compName);
        if (eitem != null) {
            return eitem.isEnable();
        }
        return null;
    }

    public static void setCompEnable(GContainer parent, String compName, boolean enable) {
        if (compName == null || parent == null) return;
        GObject eitem = parent.findByName(compName);
        if (eitem != null) {
            eitem.setEnable(enable);
        }
    }


    public static String getCompText(GContainer parent, String compName) {
        if (compName == null || parent == null) return null;
        GObject eitem = parent.findByName(compName);
        if (eitem != null) {
            return eitem.getText();
        }
        return "";
    }


    public static void setCompText(GContainer parent, String compName, String text) {
        if (compName == null || parent == null) return;
        GObject eitem = parent.findByName(compName);
        if (eitem != null) {
            eitem.setText(text);
        }
    }

    public static String getCompCmd(GContainer parent, String compName) {
        if (compName == null || parent == null) return null;
        GObject eitem = parent.findByName(compName);
        if (eitem != null) {
            return eitem.getCmd();
        }
        return "";
    }

    public static void setCompCmd(GContainer parent, String compName, String text) {
        if (compName == null || parent == null) return;
        GObject eitem = parent.findByName(compName);
        if (eitem != null) {
            eitem.setCmd(text);
        }
    }

    public static GImage getCompImage(GContainer parent, String compName) {
        if (compName == null || parent == null) return null;
        GObject eitem = parent.findByName(compName);
        if (eitem != null && eitem instanceof GImageItem) {
            return ((GImageItem) eitem).getImg();
        }
        return null;
    }

    public static void setCompImage(GContainer parent, String compName, String jarPicPath) {
        if (compName == null || parent == null) return;
        GObject eitem = parent.findByName(compName);
        if (eitem != null && eitem instanceof GImageItem) {
            ((GImageItem) eitem).setImg(getCachedImageFromJar(jarPicPath));
        }
    }

    public static void setCompImage(GContainer parent, String compName, GImage img) {
        if (compName == null || parent == null) return;
        GObject eitem = parent.findByName(compName);
        if (eitem != null && eitem instanceof GImageItem) {
            ((GImageItem) eitem).setImg(img);
        }
    }

    public static <T extends GObject> T getComponent(GContainer parent, String compName) {
        if (compName == null || parent == null) return null;
        T eitem = parent.findByName(compName);
        return eitem;
    }


    public static GActionListener getCompActionListener(GContainer parent, String compName) {
        if (compName == null || parent == null) return null;
        GObject eitem = parent.findByName(compName);
        if (eitem != null) {
            return eitem.getActionListener();
        }
        return null;
    }

    public static void setCompActionListener(GContainer parent, String compName, GActionListener listener) {
        if (compName == null || parent == null) return;
        GObject eitem = parent.findByName(compName);
        if (eitem != null) {
            eitem.setActionListener(listener);
        }
    }

    /**
     * ----------------------------------------------------------------
     * EditMenu
     * ----------------------------------------------------------------
     */

    private static EditMenu editMenu;


    static public EditMenu getEditMenu() {
        return editMenu;
    }

    static public void disposeEditMenu() {
        if (editMenu != null) editMenu.dispose();
    }

    static public class EditMenu extends GMenu {

        GTextObject text;

        public EditMenu(GForm form, float left, float top, float width, float height) {
            super(form, left, top, width, height);
        }

        @Override
        public boolean paint(long vg) {
            if (text != null && text.getParent().getForm() == null) {
                dispose();
            }
            return super.paint(vg);
        }


        synchronized void dispose() {
            GForm gf = getForm();
            if (gf != null) {
                gf.remove(editMenu);
                if (text != null) {
                    text.resetSelect();
                    text.selectMode = false;
                }
                form = null;
            }
            //System.out.println("edit menu dispose");
        }
    }

    /**
     * 唤出基于form层的编辑菜单,选中菜单项后消失,失去焦点后消失
     *
     * @param focus
     * @param x
     * @param y
     */
    synchronized static public void callEditMenu(GTextObject focus, float x, float y) {
        if (focus == null || focus.getForm() == null) {
            return;
        }
        GForm gform = focus.getForm();

        float menuH = 40, menuW = 300;

        float mx = x - menuW / 2;
        if (mx < 10) {
            mx = 10;
        } else if (mx + menuW > gform.getW()) {
            mx = gform.getW() - menuW;
        }
        mx -= gform.getX();
        float my = y - 20 - menuH;
        if (my < 20) {
            my = y + 10;
        } else if (my + menuH > gform.getH()) {
            my = gform.getH() - menuH;
        }
        my -= gform.getY();

        if (editMenu == null) {
            editMenu = new EditMenu(gform, mx, my, menuW, menuH);
            GMenuItem item;

            item = editMenu.addItem(GLanguage.getString("Select"), null);
            item.setName("EDITMENUCTX_SELECT");
            item.setActionListener(gobj -> {
                editMenu.text.doSelectText();
                setCompEnable(editMenu, "EDITMENUCTX_COPY", true);
            });
            item = editMenu.addItem(GLanguage.getString("Copy"), null);
            item.setName("EDITMENUCTX_COPY");
            item.setActionListener(gobj -> {
                editMenu.text.doCopyClipBoard();
                editMenu.dispose();
            });
            item = editMenu.addItem(GLanguage.getString("Paste"), null);
            item.setName("EDITMENUCTX_PASTE");
            item.setActionListener(gobj -> {
                if (editMenu.text.enable) {
                    editMenu.text.doPasteClipBoard();
                }
                editMenu.dispose();
            });
            item = editMenu.addItem(GLanguage.getString("Cut"), null);
            item.setName("EDITMENUCTX_CUT");
            item.setActionListener(gobj -> {
                if (editMenu.text.enable) {
                    editMenu.text.doCut();
                }
                editMenu.dispose();
            });
            item = editMenu.addItem(GLanguage.getString("SeleAll"), null);
            item.setName("EDITMENUCTX_SELECTALL");
            item.setActionListener(gobj -> {
                editMenu.text.doSelectAll();
                setCompEnable(editMenu, "EDITMENUCTX_COPY", true);
            });

            editMenu.setFixed(true);
            editMenu.setContextMenu(true);
        }
        if (focus.isEditable()) {
            setCompEnable(editMenu, "EDITMENUCTX_PASTE", true);
            setCompEnable(editMenu, "EDITMENUCTX_CUT", true);
        } else {
            setCompEnable(editMenu, "EDITMENUCTX_PASTE", false);
            setCompEnable(editMenu, "EDITMENUCTX_CUT", false);
        }
//        if (focus.selectMode) {
//            setCompEnable(editMenu, "EDITMENUCTX_COPY", true);
//        } else {
//            setCompEnable(editMenu, "EDITMENUCTX_COPY", false);
//        }
        editMenu.form = gform;
        editMenu.text = focus;
        editMenu.setLocation(mx, my);

        gform.add(editMenu);
        //System.out.println("edit menu show");
    }

    /**
     * ----------------------------------------------------------------
     * image cache
     * ----------------------------------------------------------------
     * <p>
     * 缓存图象系统, 采用weakhashmap, 自动管理图象资源
     * 当imagecache中的key没有强引用时,其value(图象)会被自动回收
     */
    static Map<String, GImage> imageCache = Collections.synchronizedMap(new WeakHashMap<>());

    /**
     * 从缓存中取得图象,如果缓存中没有,则加载
     *
     * @param filepath
     * @return
     */
    static public GImage getCachedImageFromJar(String filepath) {
        return getCachedImageFromJar(filepath, null);
    }

    /**
     * 返回 图像并且返回此图象的holder, 如果此holder不被GC销毁,此图象也不会被销毁
     *
     * @param filepath
     * @param holder
     * @return
     */
    static public GImage getCachedImageFromJar(String filepath, GAttachable holder) {
        if (filepath == null || "".equals(filepath.trim())) {
            return null;
        }
        filepath = new String(filepath);//for holder,must new
        GImage img = imageCache.get(filepath);
        if (img == null) {
            img = GImage.createImageFromJar(filepath);
            if (img != null) {
                if (holder != null) holder.setAttachment(filepath);
                imageCache.put(filepath, img);
            }
            System.out.println("load image cache " + filepath + " " + img);
        } else {
            //System.out.println("hit image from cache " + filepath);
            if (holder != null) {
                for (Map.Entry e : imageCache.entrySet()) {
                    if (filepath.equals(e.getKey())) { //虽然两个字符串字面相同,但不是同一对象
                        holder.setAttachment(e.getKey());
                    }
                }
            }
        }
        return img;
    }

    static public GImage getCachedImageFromFile(String filepath) {
        return getCachedImageFromFile(filepath, null);
    }

    static public GImage getCachedImageFromFile(String filepath, GAttachable holder) {
        if (filepath == null || "".equals(filepath.trim())) {
            return null;
        }
        filepath = new String(filepath);//for holder,must new
        GImage img = imageCache.get(filepath);
        if (img == null) {
            System.out.println("load image cache " + filepath);
            img = GImage.createImage(filepath);
            if (img != null) {
                if (holder != null) holder.setAttachment(filepath);
                imageCache.put(filepath, img);
            }
        } else {
            //System.out.println("hit image from cache " + filepath);
            if (holder != null) {
                for (Map.Entry e : imageCache.entrySet()) {
                    if (filepath.equals(e.getKey())) { //虽然两个字符串字面相同,但不是同一对象
                        holder.setAttachment(e.getKey());
                    }
                }
            }
        }
        return img;
    }
}
