static void draw_mission_notice_overlay(const Game *g, Surface *top) {
    if (!g || !top ||
        g->mission_notice_timer <= 0.0f ||
        g->mission_notice[0] == '\0') {
        return;
    }

    int w = 252;
    int x = (400 - w) / 2;
    int y = 196;

    draw_rect(top, x, y, w, 20, rgb(8, 10, 14));
    draw_rect_outline(top, x, y, w, 20, C_YELLOW);
    draw_text_center(top, 200, y + 6, g->mission_notice, 1, C_YELLOW);
}

void game_render_top(const Game *g, Surface *top) {
    if (g->mode == MODE_TITLE) {
        draw_title(g, top);
        return;
    }

    if (g->mode == MODE_SHOP) {
        draw_shop(g, top);
        return;
    }

    if (g->mode == MODE_SETTINGS) {
        draw_settings(g, top);
        return;
    }

    if (g->mode == MODE_MISSIONS) {
        draw_missions(g, top);
        return;
    }

    if (g->mode == MODE_ACHIEVEMENTS) {
        draw_achievements(g, top);
        return;
    }

    draw_world(g, top);
    draw_mission_notice_overlay(g, top);

    if (g->mode == MODE_PAUSED)
        draw_pause(g, top);
    else if (g->mode == MODE_GAMEOVER)
        draw_gameover(g, top);
}

