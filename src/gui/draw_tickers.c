/*
 * draw_tickers.c
 * 
 * Copyright 2021 chehw <hongwei.che@gmail.com>
 * 
 * The MIT License
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy of 
 * this software and associated documentation files (the "Software"), to deal in 
 * the Software without restriction, including without limitation the rights to 
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies 
 * of the Software, and to permit persons to whom the Software is furnished to 
 * do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all 
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR 
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE 
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER 
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, 
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE 
 * SOFTWARE.
 * 
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>


#include "coincheck-gui.h"

static const int s_window_size = 2400;
static const int s_max_tickers = 2000;
static const int s_image_width = s_window_size;
static const int s_image_height = 600;

void draw_tickers(panel_view_t * panel)
{
	cairo_surface_t * surface = panel->chart_ctx.surface;
	if(NULL == surface) {
		surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, s_image_width, s_image_height);
		assert(surface);
		assert(cairo_surface_status(surface) == CAIRO_STATUS_SUCCESS);
		
		panel->chart_ctx.surface = surface;
		panel->chart_ctx.image_width = s_image_width;
		panel->chart_ctx.image_height = s_image_height;
	}
	
	cairo_t * cr = NULL;
	cr = cairo_create(surface);
	cairo_set_source_rgba(cr, 0, 0, 0, 1);
	cairo_paint(cr);
	
	struct coincheck_ticker * tickers = NULL;
	ssize_t count = panel_ticker_get_lastest_history(panel->ticker_ctx, s_max_tickers, &tickers);
	if(count <= 0) {
		if(tickers) free(tickers);
		return;
	}
	double min_tick = tickers[0].low;
	double max_tick = tickers[0].high;
	double tick_scale = 1.0;
	for(ssize_t i = 1; i < count; ++i) {
		if(tickers[i].low < min_tick) min_tick = tickers[i].low;
		if(tickers[i].high > max_tick) max_tick = tickers[i].high;
	}
	
	double range = 400;
	if(max_tick - min_tick > range) {
		tick_scale = range / (max_tick - min_tick);
	}
	
	
	cairo_translate(cr, 0, range + 100);
	
	cairo_set_font_size(cr, 16);
	cairo_select_font_face(cr, "Mono", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
	char title[100] = "";
	double dashes[2] = { 2, 1 };
	
	// draw min_tick line
	cairo_move_to(cr, 0, 0);
	cairo_set_source_rgba(cr, 1, 1, 1, 1);
	snprintf(title, sizeof(title), "low: %f", min_tick);
	cairo_move_to(cr, 0, 20 + 20);
	cairo_show_text(cr, title);
	cairo_stroke(cr);
	
	cairo_set_source_rgba(cr, 0, 1, 1, 1);
	cairo_set_dash(cr, dashes, 2, 0);
	cairo_move_to(cr, 0, 0);
	cairo_line_to(cr, s_image_width, 0);
	cairo_stroke(cr);
	
	// draw max_tick line
	cairo_set_dash(cr, NULL, 0, 0);
	cairo_move_to(cr, 0, -(range + 20 + 4));
	snprintf(title, sizeof(title), "high: %f", max_tick);
	cairo_set_source_rgba(cr, 1, 1, 1, 1);
	cairo_show_text(cr, title);
	cairo_stroke(cr);
	
	cairo_set_dash(cr, dashes, 2, 0);
	cairo_set_source_rgba(cr, 0, 1, 1, 1);
	cairo_move_to(cr, 0, -(range));
	cairo_line_to(cr, s_image_width, -range);
	cairo_stroke(cr);
	
	// draw ticker
	cairo_set_dash(cr, NULL, 0, 0);
	cairo_set_line_width(cr, 2);
	cairo_set_source_rgba(cr, 1.0, 1.0, 0.0, 1.0);
	cairo_move_to(cr, 0, -(tickers[0].last - min_tick) * tick_scale);
	for(ssize_t i = 1; i < count; ++i) {
		cairo_line_to(cr, i, -(tickers[i].last - min_tick) * tick_scale);
	}
	cairo_stroke(cr);
	if(count > 0) {
		cairo_set_line_width(cr, 1);
		cairo_set_font_size(cr, 18);
		snprintf(title, sizeof(title), "%.2f", tickers[count - 1].last);
		cairo_set_source_rgba(cr, 0, 1, 0, 1);
		cairo_line_to(cr, count, -(tickers[count - 1].last - min_tick) * tick_scale);
		cairo_show_text(cr, title);
		cairo_stroke(cr);
	}
	
	cairo_destroy(cr);
	
	free(tickers);
	gtk_widget_queue_draw(panel->chart_ctx.da);
	return;
}

