/***************************************************************************

  Video Hardware for Double Dragon 3

***************************************************************************/

#include "emu.h"
#include "includes/ddragon3.h"

WRITE16_HANDLER( ddragon3_scroll_w )
{
	ddragon3_state *state = space->machine->driver_data<ddragon3_state>();

	switch (offset)
	{
		case 0: COMBINE_DATA(&state->fg_scrollx);	// Scroll X, BG1
			break;
		case 1: COMBINE_DATA(&state->fg_scrolly);	// Scroll Y, BG1
			break;
		case 2: COMBINE_DATA(&state->bg_scrollx);	// Scroll X, BG0
			break;
		case 3: COMBINE_DATA(&state->bg_scrolly);	// Scroll Y, BG0
			break;
		case 4:						// Unknown write
			break;
		case 5: flip_screen_set(space->machine, data & 0x01);	// Flip Screen
			break;
		case 6:
			COMBINE_DATA(&state->bg_tilebase);		// BG Tile Base
			state->bg_tilebase &= 0x01ff;
			tilemap_mark_all_tiles_dirty(state->bg_tilemap);
			break;
	}
}

READ16_HANDLER( ddragon3_scroll_r )
{
	ddragon3_state *state = space->machine->driver_data<ddragon3_state>();

	switch (offset)
	{
		case 0: return state->fg_scrollx;
		case 1: return state->fg_scrolly;
		case 2: return state->bg_scrollx;
		case 3: return state->bg_scrolly;
		case 5: return flip_screen_get(space->machine);
		case 6: return state->bg_tilebase;
	}

	return 0;
}

WRITE16_HANDLER( ddragon3_bg_videoram_w )
{
	ddragon3_state *state = space->machine->driver_data<ddragon3_state>();

	COMBINE_DATA(&state->bg_videoram[offset]);
	tilemap_mark_tile_dirty(state->bg_tilemap, offset);
}

WRITE16_HANDLER( ddragon3_fg_videoram_w )
{
	ddragon3_state *state = space->machine->driver_data<ddragon3_state>();

	COMBINE_DATA(&state->fg_videoram[offset]);
	tilemap_mark_tile_dirty(state->fg_tilemap, offset / 2);
}

static TILE_GET_INFO( get_bg_tile_info )
{
	ddragon3_state *state = machine->driver_data<ddragon3_state>();

	UINT16 attr = state->bg_videoram[tile_index];
	int code = (attr & 0x0fff) | ((state->bg_tilebase & 0x01) << 12);
	int color = ((attr & 0xf000) >> 12) + 16;

	SET_TILE_INFO(0, code, color, 0);
}

static TILE_GET_INFO( get_fg_tile_info )
{
	ddragon3_state *state = machine->driver_data<ddragon3_state>();

	int offs = tile_index << 1;
	UINT16 attr = state->fg_videoram[offs];
	int code = state->fg_videoram[offs + 1] & 0x1fff;
	int color = attr & 0x0f;
	int flags = (attr & 0x40) ? TILE_FLIPX : 0;

	SET_TILE_INFO(0, code, color, flags);
}

VIDEO_START( ddragon3 )
{
	ddragon3_state *state = machine->driver_data<ddragon3_state>();

	state->bg_tilemap = tilemap_create(machine, get_bg_tile_info, tilemap_scan_rows, 16, 16, 32, 32);
	state->fg_tilemap = tilemap_create(machine, get_fg_tile_info, tilemap_scan_rows, 16, 16, 32, 32);

	tilemap_set_transparent_pen(state->bg_tilemap, 0);
	tilemap_set_transparent_pen(state->fg_tilemap, 0);
}

/*
 * Sprite Format
 * ----------------------------------
 *
 * Word | Bit(s)           | Use
 * -----+-fedcba9876543210-+----------------
 *   0  | --------xxxxxxxx | ypos (signed)
 * -----+------------------+
 *   1  | --------xxx----- | height
 *   1  | -----------xx--- | yflip, xflip
 *   1  | -------------x-- | msb x
 *   1  | --------------x- | msb y?
 *   1  | ---------------x | enable
 * -----+------------------+
 *   2  | --------xxxxxxxx | tile number
 * -----+------------------+
 *   3  | --------xxxxxxxx | bank
 * -----+------------------+
 *   4  | ------------xxxx | color
 * -----+------------------+
 *   5  | --------xxxxxxxx | xpos
 * -----+------------------+
 *   6,7| unused
 */

static void draw_sprites( running_machine* machine, bitmap_t *bitmap, const rectangle *cliprect )
{
	ddragon3_state *state = machine->driver_data<ddragon3_state>();

	UINT16 *source = state->spriteram;
	UINT16 *finish = source + 0x0800;
	UINT32 bank, code, color, height, i;
	INT32 sx, sy, flipx, flipy;
	UINT16 attr;
	UINT8 flip_screen = (flip_screen_get(machine)) ? 1 : 0;

	for ( ; source < finish; source += 8)
	{
		attr = source[1];
		if (attr & 0x01)	/* enable */
		{
			bank = (source[3] & 0xff) << 8;
			code = (source[2] & 0xff) + bank;
			color = source[4] & 0x0f;
			flipx = attr & 0x10;
			flipy = attr & 0x08;
			height = (attr >> 5) & 0x07;

			sx = source[5] & 0xff;
			if (attr & 0x04)
				sx |= 0x100;
			if (sx > 383)
				sx -= 512;

			sy = source[0] & 0xff;
			sy = (attr & 0x02) ? 239 + 256 - sy : 240 - sy;

			if (flip_screen)
			{
				sx = 304 - sx;
				sy = 224 - sy;
				flipx = !flipx;
				flipy = !flipy;
			}

			for (i = 0; i <= height; i++)
				drawgfx_transpen(bitmap, cliprect, machine->gfx[1],
					code + i, color, flipx, flipy, sx, sy + (flip_screen ? i * 16 : -i * 16), 0);
		}
	}
}

VIDEO_UPDATE( ddragon3 )
{
	ddragon3_state *state = screen->machine->driver_data<ddragon3_state>();

	tilemap_set_scrollx(state->bg_tilemap, 0, state->bg_scrollx);
	tilemap_set_scrolly(state->bg_tilemap, 0, state->bg_scrolly);
	tilemap_set_scrollx(state->fg_tilemap, 0, state->fg_scrollx);
	tilemap_set_scrolly(state->fg_tilemap, 0, state->fg_scrolly);

	if ((state->vreg & 0x60) == 0x40)
	{
		tilemap_draw(bitmap, cliprect, state->bg_tilemap, TILEMAP_DRAW_OPAQUE, 0);
		tilemap_draw(bitmap, cliprect, state->fg_tilemap, 0, 0);
		draw_sprites(screen->machine, bitmap, cliprect);
	}
	else if ((state->vreg & 0x60) == 0x60)
	{
		tilemap_draw(bitmap, cliprect, state->fg_tilemap, TILEMAP_DRAW_OPAQUE, 0);
		tilemap_draw(bitmap, cliprect, state->bg_tilemap, 0, 0);
		draw_sprites(screen->machine, bitmap, cliprect);
	}
	else
	{
		tilemap_draw(bitmap, cliprect, state->bg_tilemap, TILEMAP_DRAW_OPAQUE, 0);
		draw_sprites(screen->machine, bitmap, cliprect);
		tilemap_draw(bitmap, cliprect, state->fg_tilemap, 0, 0);
	}

	return 0;
}

VIDEO_UPDATE( ctribe )
{
	ddragon3_state *state = screen->machine->driver_data<ddragon3_state>();

	tilemap_set_scrollx(state->bg_tilemap, 0, state->bg_scrollx);
	tilemap_set_scrolly(state->bg_tilemap, 0, state->bg_scrolly);
	tilemap_set_scrollx(state->fg_tilemap, 0, state->fg_scrollx);
	tilemap_set_scrolly(state->fg_tilemap, 0, state->fg_scrolly);

	if (state->vreg & 0x08)
	{
		tilemap_draw(bitmap, cliprect, state->fg_tilemap, TILEMAP_DRAW_OPAQUE, 0);
		draw_sprites(screen->machine, bitmap, cliprect);
		tilemap_draw(bitmap, cliprect, state->bg_tilemap, 0, 0);
	}
	else
	{
		tilemap_draw(bitmap, cliprect, state->bg_tilemap, TILEMAP_DRAW_OPAQUE, 0);
		tilemap_draw(bitmap, cliprect, state->fg_tilemap, 0, 0);
		draw_sprites(screen->machine, bitmap, cliprect);
	}

	return 0;
}
