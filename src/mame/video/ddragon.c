/***************************************************************************

  Video Hardware for some Technos games:
    Double Dragon, Double Dragon bootleg, Double Dragon II and China Gate

  Two Tile layers.
    Background layer is 512x512 , tiles are 16x16
    Top        layer is 256x256 , tiles are 8x8

  Sprites are 16x16, 16x32, 32x16, or 32x32 (attribute bits set dimension)


BG Tile Layout
  0          1
  ---- -xxx  xxxx xxxx  = Tile number
  --xx x---  ---- ----  = Color
  -x-- ----  ---- ----  = X Flip
  x--- ----  ---- ----  = Y Flip


Top Tile layout.
  0          1
  ---- xxxx  xxxx xxxx  = Tile number
  xxxx ----  ---- ----  = Color (China Gate)
  xxx- ----  ---- ----  = Color (Double Dragon)


Sprite layout.
  0          1          2          3          4
  ---- ----  ---- ----  ---- xxxx  xxxx xxxx  ---- ----  = Sprite number
  ---- ----  ---- ----  xxxx ----  ---- ----  ---- ----  = Color
  xxxx xxxx  ---- ----  ---- ----  ---- ----  ---- ----  = Y position
  ---- ----  ---- ---x  ---- ----  ---- ----  ---- ----  = Y MSb position ???
  ---- ----  ---- ----  ---- ----  ---- ----  xxxx xxxx  = X position
  ---- ----  ---- --x-  ---- ----  ---- ----  ---- ----  = X position Msb
  ---- ----  ---- -x--  ---- ----  ---- ----  ---- ----  = Y Flip
  ---- ----  ---- x---  ---- ----  ---- ----  ---- ----  = X Flip
  ---- ----  --xx ----  ---- ----  ---- ----  ---- ----  = Sprite Dimension
  ---- ----  x--- ----  ---- ----  ---- ----  ---- ----  = Visible

***************************************************************************/

#include "emu.h"
#include "includes/ddragon.h"


/***************************************************************************

  Callbacks for the TileMap code

***************************************************************************/

static TILEMAP_MAPPER( background_scan )
{
	/* logical (col,row) -> memory offset */
	return (col & 0x0f) | ((row & 0x0f) << 4) | ((col & 0x10) << 4) | ((row & 0x10) << 5);
}

static TILE_GET_INFO( get_bg_tile_info )
{
	ddragon_state *state = machine->driver_data<ddragon_state>();

	tile_index <<= 1;
	UINT8 attr = state->bgvideoram[tile_index];
	SET_TILE_INFO(	2,
			state->bgvideoram[tile_index | 0x01] | ((attr & 0x07) << 8),
			(attr >> 3) & 0x07, TILE_FLIPYX((attr & 0xc0) >> 6) );
}

static TILE_GET_INFO( get_fg_tile_info )
{
	ddragon_state *state = machine->driver_data<ddragon_state>();

	tile_index <<= 1;
	UINT8 attr = state->fgvideoram[tile_index];
	SET_TILE_INFO(	0,
			state->fgvideoram[tile_index | 0x01] | ((attr & 0x07) << 8),
			attr >> 5, 0 );
}

static TILE_GET_INFO( get_fg_16color_tile_info )
{
	ddragon_state *state = machine->driver_data<ddragon_state>();

	tile_index <<= 1;
	UINT8 attr = state->fgvideoram[tile_index];
	SET_TILE_INFO(	0,
			state->fgvideoram[tile_index | 0x01] | ((attr & 0x0f) << 8),
			attr >> 4, 0 );
}


/***************************************************************************

  Start the video hardware emulation.

***************************************************************************/

VIDEO_START( ddragon )
{
	ddragon_state *state = machine->driver_data<ddragon_state>();

	state->bg_tilemap = tilemap_create(machine, get_bg_tile_info, background_scan, 16, 16, 32, 32);
	state->fg_tilemap = tilemap_create(machine, get_fg_tile_info, tilemap_scan_rows, 8, 8, 32, 32);

	tilemap_set_transparent_pen(state->fg_tilemap, 0);
	tilemap_set_scrolldx(state->fg_tilemap, 0, 384 - 256);
	tilemap_set_scrolldx(state->bg_tilemap, 0, 384 - 256);
	tilemap_set_scrolldy(state->fg_tilemap, -8, -8);
	tilemap_set_scrolldy(state->bg_tilemap, -8, -8);
}

VIDEO_START( chinagat )
{
	ddragon_state *state = machine->driver_data<ddragon_state>();

	state->bg_tilemap = tilemap_create(machine, get_bg_tile_info,background_scan, 16, 16, 32, 32);
	state->fg_tilemap = tilemap_create(machine, get_fg_16color_tile_info,tilemap_scan_rows, 8, 8, 32, 32);

	tilemap_set_transparent_pen(state->fg_tilemap, 0);
	tilemap_set_scrolldy(state->fg_tilemap, -8, -8);
	tilemap_set_scrolldy(state->bg_tilemap, -8, -8);
}


/***************************************************************************

  Memory handlers

***************************************************************************/

WRITE8_HANDLER( ddragon_bgvideoram_w )
{
	ddragon_state *state = space->machine->driver_data<ddragon_state>();

	state->bgvideoram[offset] = data;
	tilemap_mark_tile_dirty(state->bg_tilemap, offset >> 1);
}

WRITE8_HANDLER( ddragon_fgvideoram_w )
{
	ddragon_state *state = space->machine->driver_data<ddragon_state>();

	state->fgvideoram[offset] = data;
	tilemap_mark_tile_dirty(state->fg_tilemap, offset >> 1);
}


/***************************************************************************

  Display refresh

***************************************************************************/

#define DRAW_SPRITE(order, sx, sy)	drawgfx_transpen(bitmap,	\
					cliprect, gfx,			\
					(which + order), color, flipx, flipy, sx, sy, 0);


static void draw_sprites(running_machine *machine, bitmap_t *bitmap, const rectangle *cliprect)
{
	ddragon_state *state = machine->driver_data<ddragon_state>();

	const gfx_element *gfx = machine->gfx[1];
	const UINT8 *src = (UINT8 *)state->spriteram;
	INT32 attr, sx, sy, size, flipx, flipy, dx, dy, which, color;

	for (UINT32 i = 0; i < 64 * 5; i += 5)
	{
		attr = src[i + 1];
		if (attr & 0x80)  /* visible */
		{
			sx = 240 - src[i + 4] + ((attr & 0x02) << 7);
			sy = 232 - src[i + 0] + ((attr & 0x01) << 8);
			size = (attr & 0x30) >> 4;
			flipx = attr & 0x08;
			flipy = attr & 0x04;
			dx = dy = -16;

			if (state->technos_video_hw == 2)		/* Double Dragon 2 */
			{
				color = src[i + 2] >> 5;
				which = src[i + 3] | ((src[i + 2] & 0x1f) << 8);
			}
			else
			{
				color = src[i + 2] >> 4;
				which = src[i + 3] | ((src[i + 2] & 0x0f) << 8);

				if (state->technos_video_hw == 1)	/* China Gate */
				{
					/* fix sprite clip */
					if ((sx < -7) && (sx > -16))
						sx += 256;

					/* fix sprite clip */
					if ((sy < -7) && (sy > -16))
						sy += 256;
				}
			}

			if (flip_screen_get(machine))
			{
				sx = 240 - sx;
				sy = 256 - sy;
				flipx = !flipx;
				flipy = !flipy;
				dx = -dx;
				dy = -dy;
			}

			which &= ~size;

			switch (size)
			{
				case 0:	/* normal */
				DRAW_SPRITE(0, sx, sy);
				break;

				case 1:	/* double y */
				DRAW_SPRITE(0, sx, sy + dy);
				DRAW_SPRITE(1, sx, sy);
				break;

				case 2:	/* double x */
				DRAW_SPRITE(0, sx + dx, sy);
				DRAW_SPRITE(2, sx, sy);
				break;

				case 3:
				DRAW_SPRITE(0, sx + dx, sy + dy);
				DRAW_SPRITE(1, sx + dx, sy);
				DRAW_SPRITE(2, sx, sy + dy);
				DRAW_SPRITE(3, sx, sy);
				break;
			}
		}
	}
}

#undef DRAW_SPRITE


VIDEO_UPDATE( ddragon )
{
	ddragon_state *state = screen->machine->driver_data<ddragon_state>();

	int scrollx = (state->scrollx_hi << 8) | *state->scrollx_lo;
	int scrolly = (state->scrolly_hi << 8) | *state->scrolly_lo;

	tilemap_set_scrollx(state->bg_tilemap, 0, scrollx);
	tilemap_set_scrolly(state->bg_tilemap, 0, scrolly);

	tilemap_draw(bitmap, cliprect, state->bg_tilemap, 0, 0);
	draw_sprites(screen->machine, bitmap, cliprect);
	tilemap_draw(bitmap, cliprect, state->fg_tilemap, 0, 0);

	return 0;
}
