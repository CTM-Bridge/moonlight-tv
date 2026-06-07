#include "app.h"
#include "app_session.h"
#include "streaming.controller.h"
#include "stream/video/session_video.h"
#include "ui/root.h"
#include "ui/common/progress_dialog.h"
#include "lvgl/lv_ext_utils.h"

#include "util/user_event.h"
#include "util/i18n.h"
#include "logging.h"

#include "ctm_bridge_glue.h"

static void exit_streaming(lv_event_t *event);

static void suspend_streaming(lv_event_t *event);

static void open_keyboard(lv_event_t *event);

static void toggle_vmouse(lv_event_t *event);

static void hide_overlay(lv_event_t *event);

static bool show_overlay(streaming_controller_t *controller);

static void on_view_created(lv_fragment_t *self, lv_obj_t *view);

static void on_delete_obj(lv_fragment_t *self, lv_obj_t *view);

static void on_obj_deleted(lv_fragment_t *self, lv_obj_t *view);

static bool on_event(lv_fragment_t *self, int code, void *userdata);

static void constructor(lv_fragment_t *self, void *args);

static void controller_dtor(lv_fragment_t *self);

static void overlay_key_cb(lv_event_t *e);

static void update_buttons_layout(streaming_controller_t *controller);

static void pin_toggle(lv_event_t *e);

static void open_ctm_panel(lv_event_t *e);

static void ctm_panel_populate(lv_obj_t *list);

static void ctm_panel_dev_cb(lv_event_t *e);

static void ctm_panel_resize_cb(lv_event_t *e);

static void ctm_panel_plugall_cb(lv_event_t *e);

static void ctm_panel_unplugall_cb(lv_event_t *e);

static void ctm_panel_settings_cb(lv_event_t *e);

static void ctm_settings_apply_cb(lv_event_t *e);

static void open_ctm_settings(int index);

const lv_fragment_class_t streaming_controller_class = {
        .constructor_cb = constructor,
        .destructor_cb = controller_dtor,
        .create_obj_cb = streaming_scene_create,
        .obj_created_cb = on_view_created,
        .obj_will_delete_cb = on_delete_obj,
        .obj_deleted_cb = on_obj_deleted,
        .event_cb = on_event,
        .instance_size = sizeof(streaming_controller_t),
};

static bool overlay_showing = false, overlay_pinned = false;
static streaming_controller_t *current_controller = NULL;

bool streaming_overlay_shown() {
    return overlay_showing;
}

bool streaming_stats_shown() {
    return overlay_showing || overlay_pinned;
}

bool streaming_refresh_stats() {
    streaming_controller_t *controller = current_controller;
    if (!controller) { return false; }
    if (!streaming_stats_shown()) {
        return false;
    }
    app_t *app = controller->global;
    const struct VIDEO_STATS *dst = &vdec_summary_stats;
    const struct VIDEO_INFO *info = &vdec_stream_info;
    if (info->width > 0 && info->height > 0) {
        lv_label_set_text_fmt(controller->stats_items.decoder, "%s, %d\u00d7%d (%s)", vdec_stream_info.format, info->width,
                              info->height, SS4S_ModuleInfoGetId(app->ss4s.selection.video_module));
    } else {
        lv_label_set_text_fmt(controller->stats_items.decoder, "%s (%s)", vdec_stream_info.format,
                              SS4S_ModuleInfoGetId(app->ss4s.selection.video_module));
    }
    lv_label_set_text_fmt(controller->stats_items.audio, "%s, %s (%s)", audio_stream_info.format,
                          audio_stream_info.channels, SS4S_ModuleInfoGetId(app->ss4s.selection.audio_module));
    lv_label_set_text_fmt(controller->stats_items.rtt, "%d ms (var. %d ms)", dst->rtt, dst->rttVariance);
    lv_label_set_text_fmt(controller->stats_items.net_fps, "%.2f FPS", dst->receivedFps);

    if (dst->submittedFrames) {
        lv_label_set_text_fmt(controller->stats_items.drop_rate, "%.2f%%",
                              (float) dst->networkDroppedFrames / (float) dst->totalFrames * 100);
        if (vdec_stream_info.has_host_latency) {
            float avgCapLatency = (float) dst->totalCaptureLatency / (float) dst->submittedFrames / 10.0f;
            lv_label_set_text_fmt(controller->stats_items.host_latency, "avg %.2f ms", avgCapLatency);
        } else {
            lv_label_set_text_fmt(controller->stats_items.host_latency, "not available");
        }
        if (vdec_stream_info.has_decoder_latency) {
            float avgSubmitTime = (float) dst->totalSubmitTime / (float) dst->submittedFrames;
            lv_label_set_text_fmt(controller->stats_items.vdec_latency, "avg %.2f ms",
                                  avgSubmitTime + dst->avgDecoderLatency);
        } else {
            lv_label_set_text_fmt(controller->stats_items.vdec_latency, "not available");
        }
    } else {
        lv_label_set_text(controller->stats_items.drop_rate, "-");
        lv_label_set_text_fmt(controller->stats_items.host_latency, "-");
        lv_label_set_text_fmt(controller->stats_items.vdec_latency, "-");
    }
    return true;
}

void streaming_notice_show(const char *message) {
    streaming_controller_t *controller = current_controller;
    if (!controller) { return; }
    lv_label_set_text(controller->notice_label, message);
    if (message && message[0]) {
        lv_obj_clear_flag(controller->notice, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(controller->notice, LV_OBJ_FLAG_HIDDEN);
    }
}

static void constructor(lv_fragment_t *self, void *args) {
    streaming_controller_t *controller = (streaming_controller_t *) self;
    current_controller = controller;

    overlay_showing = false;

    streaming_styles_init(controller);

    const streaming_scene_arg_t *arg = (streaming_scene_arg_t *) args;
    controller->global = arg->global;
    app_session_begin(arg->global, &arg->uuid, &arg->app);
}

static void controller_dtor(lv_fragment_t *self) {
    streaming_controller_t *fragment = (streaming_controller_t *) self;
    streaming_styles_reset(fragment);
    if (current_controller == fragment) {
        current_controller = NULL;
    }
}

static bool on_event(lv_fragment_t *self, int code, void *userdata) {
    LV_UNUSED(userdata);
    streaming_controller_t *controller = (streaming_controller_t *) self;
    switch (code) {
        case USER_STREAM_CONNECTING: {
            controller->progress = progress_dialog_create(locstr("Connecting..."));
            if (lv_obj_check_type(controller->progress->parent, &lv_msgbox_backdrop_class)) {
                lv_obj_set_style_bg_opa(controller->progress->parent, LV_OPA_TRANSP, 0);
            }
            lv_obj_set_width(controller->progress, LV_DPX(300));
            lv_obj_set_height(controller->progress, LV_DPX(100));
            lv_obj_add_flag(controller->overlay, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(controller->hint, LV_OBJ_FLAG_HIDDEN);
            return true;
        }
        case USER_STREAM_OPEN: {
            if (controller->progress) {
                lv_msgbox_close(controller->progress);
                controller->progress = NULL;
            }
            lv_obj_add_flag(controller->overlay, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(controller->hint, LV_OBJ_FLAG_HIDDEN);
            break;
        }
        case USER_STREAM_CLOSE: {
            controller->progress = progress_dialog_create(locstr("Disconnecting..."));
            lv_obj_add_flag(controller->overlay, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(controller->stats, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(controller->hint, LV_OBJ_FLAG_HIDDEN);
            break;
        }
        case USER_STREAM_FINISHED: {
            if (controller->progress) {
                lv_msgbox_close(controller->progress);
                controller->progress = NULL;
            }
            lv_async_call((lv_async_cb_t) lv_fragment_del, controller);
            break;
        }
        case USER_OPEN_OVERLAY: {
            show_overlay(controller);
            return true;
        }
        case USER_SIZE_CHANGED: {
            update_buttons_layout(controller);
            streaming_overlay_resized(controller);
            return false;
        }
        default: {
            break;
        }
    }
    return false;
}

static void on_view_created(lv_fragment_t *self, lv_obj_t *view) {
    streaming_controller_t *controller = (streaming_controller_t *) self;
    app_input_set_group(&controller->global->ui.input, controller->group);
    lv_obj_add_event_cb(controller->quit_btn, exit_streaming, LV_EVENT_CLICKED, self);
    lv_obj_add_event_cb(controller->suspend_btn, suspend_streaming, LV_EVENT_CLICKED, self);
    lv_obj_add_event_cb(controller->kbd_btn, open_keyboard, LV_EVENT_CLICKED, self);
    lv_obj_add_event_cb(controller->vmouse_btn, toggle_vmouse, LV_EVENT_CLICKED, self);
    lv_obj_add_event_cb(controller->ctm_btn, open_ctm_panel, LV_EVENT_CLICKED, self);
    lv_obj_add_event_cb(controller->base.obj, hide_overlay, LV_EVENT_CLICKED, self);
    lv_obj_add_event_cb(controller->overlay, overlay_key_cb, LV_EVENT_KEY, controller);
    lv_obj_add_event_cb(controller->base.obj, hide_overlay, LV_EVENT_CANCEL, controller);

    lv_obj_t *notice = lv_obj_create(lv_layer_sys());
    lv_obj_set_size(notice, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_align(notice, LV_ALIGN_TOP_RIGHT, -LV_DPX(20), LV_DPX(20));
    lv_obj_set_style_radius(notice, LV_DPX(5), 0);
    lv_obj_set_style_pad_hor(notice, LV_DPX(5), 0);
    lv_obj_set_style_pad_ver(notice, LV_DPX(3), 0);
    lv_obj_set_style_border_opa(notice, LV_OPA_TRANSP, 0);
    lv_obj_set_style_bg_opa(notice, LV_OPA_40, 0);
    lv_obj_set_style_bg_color(notice, lv_color_black(), 0);
    lv_obj_t *notice_label = lv_label_create(notice);
    lv_obj_set_size(notice_label, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_text_font(notice_label, lv_theme_get_font_small(view), 0);
    lv_obj_add_flag(notice, LV_OBJ_FLAG_HIDDEN);

    controller->notice = notice;
    controller->notice_label = notice_label;

    lv_obj_add_event_cb(controller->stats_pin, pin_toggle, LV_EVENT_VALUE_CHANGED, controller->stats);

#if !defined(TARGET_WEBOS)
    const app_settings_t *settings = &controller->global->settings;
    if (settings->syskey_capture) {
        SDL_SetWindowGrab(controller->global->ui.window, SDL_TRUE);
    }
#endif
}

static void on_delete_obj(lv_fragment_t *self, lv_obj_t *view) {
    LV_UNUSED(view);
    streaming_controller_t *controller = (streaming_controller_t *) self;
    if (controller->notice) {
        lv_obj_del(controller->notice);
    }
    if (controller->stats->parent != controller->overlay) {
        lv_obj_del(controller->stats);
    }
    app_input_set_group(&controller->global->ui.input, NULL);
    lv_group_del(controller->group);

#if !defined(TARGET_WEBOS)
    SDL_SetWindowGrab(controller->global->ui.window, SDL_FALSE);
#endif
}

static void on_obj_deleted(lv_fragment_t *self, lv_obj_t *view) {
    LV_UNUSED(view);
    streaming_controller_t *controller = (streaming_controller_t *) self;
    lv_obj_del(controller->detached_root);
}

static void exit_streaming(lv_event_t *event) {
    streaming_controller_t *self = lv_event_get_user_data(event);
    session_interrupt(self->global->session, true, STREAMING_INTERRUPT_USER);
}

static void suspend_streaming(lv_event_t *event) {
    streaming_controller_t *self = lv_event_get_user_data(event);
    session_interrupt(self->global->session, false, STREAMING_INTERRUPT_USER);
}

static void open_keyboard(lv_event_t *event) {
    streaming_controller_t *controller = lv_event_get_user_data(event);
    hide_overlay(event);
    app_t *app = controller->global;
    app_start_text_input(&app->ui.input, 0, app->ui.height / 2 - 40, app->ui.width, 40);
    session_screen_keyboard_opened(app->session);
}

static void toggle_vmouse(lv_event_t *event) {
    streaming_controller_t *controller = lv_event_get_user_data(event);
    hide_overlay(event);
    app_t *app = controller->global;
    session_toggle_vmouse(app->session);
}

static void ctm_panel_populate(lv_obj_t *list) {
    lv_obj_clean(list);

    lv_obj_t *pall = lv_list_add_btn(list, NULL, locstr("Plug ALL"));
    lv_obj_set_style_bg_color(pall, lv_palette_main(LV_PALETTE_GREEN), 0);
    lv_obj_set_style_bg_opa(pall, LV_OPA_COVER, 0);
    lv_obj_add_event_cb(pall, ctm_panel_plugall_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *uall = lv_list_add_btn(list, NULL, locstr("Unplug ALL"));
    lv_obj_set_style_bg_color(uall, lv_palette_main(LV_PALETTE_RED), 0);
    lv_obj_set_style_bg_opa(uall, LV_OPA_COVER, 0);
    lv_obj_add_event_cb(uall, ctm_panel_unplugall_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *sett = lv_list_add_btn(list, NULL, locstr("DS4/DS5 settings"));
    lv_obj_set_style_bg_color(sett, lv_palette_darken(LV_PALETTE_BLUE, 1), 0);
    lv_obj_set_style_bg_opa(sett, LV_OPA_COVER, 0);
    lv_obj_add_event_cb(sett, ctm_panel_settings_cb, LV_EVENT_CLICKED, NULL);

    ctm_bridge_dev_t devs[16];
    int n = ctm_bridge_list(devs, 16);
    if (n == 0) {
        lv_list_add_text(list, locstr("No controllers detected"));
        return;
    }
    for (int i = 0; i < n; ++i) {
        char txt[224];
        snprintf(txt, sizeof(txt), "%s  [%s:%s]  %s%s",
                 devs[i].name, devs[i].vid, devs[i].pid, devs[i].kind,
                 devs[i].plugged ? "   (PLUGGED)" : "");
        lv_obj_t *btn = lv_list_add_btn(list, NULL, txt);
        lv_obj_add_flag(btn, LV_OBJ_FLAG_EVENT_BUBBLE);
        lv_obj_set_style_radius(btn, LV_DPX(8), 0);
        lv_obj_set_style_pad_all(btn, LV_DPX(14), 0);
        lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(btn, devs[i].plugged
                                  ? lv_palette_main(LV_PALETTE_GREEN)
                                  : lv_palette_darken(LV_PALETTE_BLUE_GREY, 3), 0);
        lv_obj_set_style_bg_color(btn, lv_palette_darken(LV_PALETTE_BLUE, 2), LV_STATE_FOCUS_KEY);
        lv_obj_add_event_cb(btn, ctm_panel_dev_cb, LV_EVENT_CLICKED, (void *) (intptr_t) devs[i].index);
    }
}

static void ctm_panel_dev_cb(lv_event_t *e) {
    lv_obj_t *btn = lv_event_get_target(e);
    lv_obj_t *list = lv_obj_get_parent(btn);
    int gindex = (int) (intptr_t) lv_event_get_user_data(e);
    ctm_bridge_dev_t devs[16];
    int n = ctm_bridge_list(devs, 16);
    for (int i = 0; i < n; ++i) {
        if (devs[i].index == gindex) {
            if (devs[i].plugged) {
                ctm_bridge_unplug_index(gindex);
            } else {
                ctm_bridge_plug_index(gindex);
            }
            break;
        }
    }
    ctm_panel_populate(list);
}

// Give the msgbox content a real height (the list is 100% of it). Without this the
// content sizes to its children and the list collapses to nothing. Mirrors
// help.dialog.c's resize handler.
static void ctm_panel_resize_cb(lv_event_t *e) {
    lv_obj_t *msgbox = lv_event_get_current_target(e);
    lv_obj_t *title = lv_msgbox_get_title(msgbox);
    lv_obj_t *close_btn = lv_msgbox_get_close_btn(msgbox);
    lv_coord_t title_height = LV_MAX(lv_obj_get_height(title), lv_obj_get_height(close_btn));
    lv_obj_t *content = lv_msgbox_get_content(msgbox);
    lv_coord_t height = lv_obj_get_content_height(msgbox);
    lv_obj_set_height(content, height - title_height);
}

/* --- DS4/DS5 per-controller settings dialog ----------------------------------- */
static int s_set_index = -1;
static lv_obj_t *s_set_audio, *s_set_hvol, *s_set_svol, *s_set_lat, *s_set_hap;

static void ctm_panel_plugall_cb(lv_event_t *e) {
    lv_obj_t *list = lv_obj_get_parent(lv_event_get_target(e));
    ctm_bridge_plug_all();
    ctm_panel_populate(list);
}

static void ctm_panel_unplugall_cb(lv_event_t *e) {
    lv_obj_t *list = lv_obj_get_parent(lv_event_get_target(e));
    ctm_bridge_unplug_all();
    ctm_panel_populate(list);
}

static void ctm_settings_apply_cb(lv_event_t *e) {
    LV_UNUSED(e);
    if (s_set_index < 0) {
        return;
    }
    ctm_bridge_settings_t s;
    if (!ctm_bridge_get_settings(s_set_index, &s)) {
        return;
    }
    s.audio_mode = (int) lv_dropdown_get_selected(s_set_audio);
    s.headset_volume_percent = (int) lv_slider_get_value(s_set_hvol);
    s.speaker_volume_percent = (int) lv_slider_get_value(s_set_svol);
    s.latency_ms = (int) lv_slider_get_value(s_set_lat);
    s.haptics_gain_centi = (int) lv_slider_get_value(s_set_hap);
    ctm_bridge_set_settings(s_set_index, &s);
}

static lv_obj_t *ctm_settings_slider(lv_obj_t *parent, const char *label, int val, int min, int max) {
    lv_obj_t *l = lv_label_create(parent);
    lv_label_set_text(l, label);
    lv_obj_t *sl = lv_slider_create(parent);
    lv_obj_set_width(sl, LV_PCT(90));
    lv_slider_set_range(sl, min, max);
    lv_slider_set_value(sl, val, LV_ANIM_OFF);
    lv_obj_add_event_cb(sl, ctm_settings_apply_cb, LV_EVENT_VALUE_CHANGED, NULL);
    return sl;
}

static void open_ctm_settings(int index) {
    ctm_bridge_settings_t s;
    if (!ctm_bridge_get_settings(index, &s)) {
        return;
    }
    s_set_index = index;
    lv_obj_t *mb = lv_msgbox_create(NULL, locstr("Controller Settings"), NULL, NULL, true);
    lv_obj_add_event_cb(mb, ctm_panel_resize_cb, LV_EVENT_SIZE_CHANGED, NULL);
    lv_obj_set_size(mb, LV_PCT(70), LV_PCT(85));
    lv_obj_t *c = lv_msgbox_get_content(mb);
    lv_obj_set_flex_flow(c, LV_FLEX_FLOW_COLUMN);

    lv_obj_t *al = lv_label_create(c);
    lv_label_set_text(al, locstr("Audio mode"));
    s_set_audio = lv_dropdown_create(c);
    lv_dropdown_set_options(s_set_audio, "Auto\nOff\nSpeaker\nHeadset\nBoth");
    lv_dropdown_set_selected(s_set_audio, (uint16_t) s.audio_mode);
    lv_obj_set_width(s_set_audio, LV_PCT(90));
    lv_obj_add_event_cb(s_set_audio, ctm_settings_apply_cb, LV_EVENT_VALUE_CHANGED, NULL);

    s_set_hvol = ctm_settings_slider(c, locstr("Headset volume %"), s.headset_volume_percent, 0, 100);
    s_set_svol = ctm_settings_slider(c, locstr("Speaker volume %"), s.speaker_volume_percent, 0, 100);
    s_set_lat = ctm_settings_slider(c, locstr("Latency (ms)"), s.latency_ms, 20, 255);
    s_set_hap = ctm_settings_slider(c, locstr("Haptics gain (x0.01)"), s.haptics_gain_centi, 0, 500);
    lv_obj_center(mb);
}

static void ctm_panel_settings_cb(lv_event_t *e) {
    LV_UNUSED(e);
    ctm_bridge_dev_t devs[16];
    int n = ctm_bridge_list(devs, 16);
    for (int i = 0; i < n; ++i) {
        if (devs[i].plugged &&
            (strcmp(devs[i].kind, "ds5") == 0 || strcmp(devs[i].kind, "ds4") == 0)) {
            open_ctm_settings(devs[i].index);
            return;
        }
    }
}

static void open_ctm_panel(lv_event_t *event) {
    LV_UNUSED(event);
    // Modal msgbox with a built-in close button; the moonlight theme manages the
    // modal input group. Lists detected controllers; tap one to plug/unplug it.
    lv_obj_t *msgbox = lv_msgbox_create(NULL, locstr("CTM Bridge"), NULL, NULL, true);
    lv_obj_add_event_cb(msgbox, ctm_panel_resize_cb, LV_EVENT_SIZE_CHANGED, NULL);
    lv_obj_set_size(msgbox, LV_PCT(80), LV_PCT(80));
    lv_obj_t *content = lv_msgbox_get_content(msgbox);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
    lv_obj_t *list = lv_list_create(content);
    lv_obj_set_size(list, LV_PCT(100), LV_PCT(100));
    ctm_panel_populate(list);
    lv_obj_center(msgbox);
}

bool show_overlay(streaming_controller_t *controller) {
    if (overlay_showing) {
        return false;
    }
    overlay_showing = true;
    lv_obj_clear_flag(controller->base.obj, LV_OBJ_FLAG_HIDDEN);

    lv_area_t coords = controller->video->coords;
    streaming_enter_overlay(controller->global->session, coords.x1, coords.y1, lv_area_get_width(&coords),
                            lv_area_get_height(&coords));
    streaming_refresh_stats();

    app_stop_text_input(&controller->global->ui.input);

    update_buttons_layout(controller);
    return true;
}

static void hide_overlay(lv_event_t *event) {
    streaming_controller_t *controller = (streaming_controller_t *) lv_event_get_user_data(event);
    app_input_set_button_points(&controller->global->ui.input, NULL);
    lv_obj_add_flag(controller->base.obj, LV_OBJ_FLAG_HIDDEN);
    if (!overlay_showing) {
        return;
    }
    overlay_showing = false;
    app_set_mouse_grab(&global->input, true);
    streaming_enter_fullscreen(controller->global->session);
}

static void overlay_key_cb(lv_event_t *e) {
    streaming_controller_t *controller = lv_event_get_user_data(e);
    lv_group_t *group = controller->group;
    switch (lv_event_get_key(e)) {
        case LV_KEY_LEFT:
            lv_group_focus_prev(group);
            break;
        case LV_KEY_RIGHT:
            lv_group_focus_next(group);
            break;
        default:
            break;
    }
}

static void update_buttons_layout(streaming_controller_t *controller) {
    lv_area_t coords;
    lv_obj_get_coords(controller->quit_btn, &coords);
    lv_area_center(&coords, &controller->button_points[1]);
    lv_obj_get_coords(controller->suspend_btn, &coords);
    lv_area_center(&coords, &controller->button_points[3]);
    lv_obj_get_coords(controller->kbd_btn, &coords);
    lv_area_center(&coords, &controller->button_points[4]);
    app_input_set_button_points(&controller->global->ui.input, controller->button_points);
}

static void pin_toggle(lv_event_t *e) {
    lv_obj_t *toggle_view = lv_event_get_user_data(e);
    lv_fragment_t *fragment = lv_obj_get_user_data(toggle_view);
    bool checked = lv_obj_has_state(lv_event_get_current_target(e), LV_STATE_CHECKED);
    bool pinned = toggle_view->parent != fragment->obj;
    overlay_pinned = checked;
    if (checked == pinned) {
        return;
    }
    if (checked) {
        lv_obj_set_parent(toggle_view, lv_layer_top());
        lv_obj_add_state(toggle_view, LV_STATE_USER_1);
    } else {
        lv_obj_set_parent(toggle_view, fragment->obj);
        lv_obj_clear_state(toggle_view, LV_STATE_USER_1);
    }
}
