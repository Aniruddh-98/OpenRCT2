/*****************************************************************************
 * Copyright (c) 2014-2019 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#include <algorithm>
#include <openrct2-ui/interface/Viewport.h>
#include <openrct2-ui/interface/Widget.h>
#include <openrct2-ui/windows/Window.h>
#include <openrct2/Cheats.h>
#include <openrct2/Context.h>
#include <openrct2/Game.h>
#include <openrct2/Input.h>
#include <openrct2/actions/TrackDesignAction.h>
#include <openrct2/audio/audio.h>
#include <openrct2/localisation/Localisation.h>
#include <openrct2/ride/Track.h>
#include <openrct2/ride/TrackData.h>
#include <openrct2/ride/TrackDesign.h>
#include <openrct2/ride/TrackDesignRepository.h>
#include <openrct2/sprites.h>
#include <openrct2/windows/Intent.h>
#include <openrct2/world/Park.h>
#include <openrct2/world/Surface.h>
#include <vector>

constexpr int16_t TRACK_MINI_PREVIEW_WIDTH = 168;
constexpr int16_t TRACK_MINI_PREVIEW_HEIGHT = 78;
constexpr uint16_t TRACK_MINI_PREVIEW_SIZE = TRACK_MINI_PREVIEW_WIDTH * TRACK_MINI_PREVIEW_HEIGHT;

struct rct_track_td6;

static constexpr uint8_t _PaletteIndexColourEntrance = PALETTE_INDEX_20; // White
static constexpr uint8_t _PaletteIndexColourExit = PALETTE_INDEX_10;     // Black
static constexpr uint8_t _PaletteIndexColourTrack = PALETTE_INDEX_248;   // Grey (dark)
static constexpr uint8_t _PaletteIndexColourStation = PALETTE_INDEX_252; // Grey (light)

// clang-format off
enum {
    WIDX_BACKGROUND,
    WIDX_TITLE,
    WIDX_CLOSE,
    WIDX_ROTATE,
    WIDX_MIRROR,
    WIDX_SELECT_DIFFERENT_DESIGN,
    WIDX_PRICE
};

validate_global_widx(WC_TRACK_DESIGN_PLACE, WIDX_ROTATE);

static rct_widget window_track_place_widgets[] = {
    { WWT_FRAME,            0,  0,      199,    0,      123,    0xFFFFFFFF,                     STR_NONE                                    },
    { WWT_CAPTION,          0,  1,      198,    1,      14,     STR_STRING,                     STR_WINDOW_TITLE_TIP                        },
    { WWT_CLOSEBOX,         0,  187,    197,    2,      13,     STR_CLOSE_X,                    STR_CLOSE_WINDOW_TIP                        },
    { WWT_FLATBTN,          0,  173,    196,    83,     106,    SPR_ROTATE_ARROW,               STR_ROTATE_90_TIP                           },
    { WWT_FLATBTN,          0,  173,    196,    59,     82,     SPR_MIRROR_ARROW,               STR_MIRROR_IMAGE_TIP                        },
    { WWT_BUTTON,           0,  4,      195,    109,    120,    STR_SELECT_A_DIFFERENT_DESIGN,  STR_GO_BACK_TO_DESIGN_SELECTION_WINDOW_TIP  },
    { WWT_EMPTY,            0,  0,      0,      0,      0,      0xFFFFFFFF,                     STR_NONE                                    },
    { WIDGETS_END },
};

static void window_track_place_close(rct_window *w);
static void window_track_place_mouseup(rct_window *w, rct_widgetindex widgetIndex);
static void window_track_place_update(rct_window *w);
static void window_track_place_toolupdate(rct_window* w, rct_widgetindex widgetIndex, ScreenCoordsXY screenCoords);
static void window_track_place_tooldown(rct_window* w, rct_widgetindex widgetIndex, ScreenCoordsXY screenCoords);
static void window_track_place_toolabort(rct_window *w, rct_widgetindex widgetIndex);
static void window_track_place_unknown14(rct_window *w);
static void window_track_place_invalidate(rct_window *w);
static void window_track_place_paint(rct_window *w, rct_drawpixelinfo *dpi);

static rct_window_event_list window_track_place_events = {
    window_track_place_close,
    window_track_place_mouseup,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    window_track_place_update,
    nullptr,
    nullptr,
    window_track_place_toolupdate,
    window_track_place_tooldown,
    nullptr,
    nullptr,
    window_track_place_toolabort,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    window_track_place_unknown14,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    window_track_place_invalidate,
    window_track_place_paint,
    nullptr
};
// clang-format on

static std::vector<uint8_t> _window_track_place_mini_preview;
static int16_t _window_track_place_last_x;
static int16_t _window_track_place_last_y;

static uint8_t _window_track_place_ride_index;
static bool _window_track_place_last_was_valid;
static int16_t _window_track_place_last_valid_x;
static int16_t _window_track_place_last_valid_y;
static int16_t _window_track_place_last_valid_z;
static money32 _window_track_place_last_cost;

static std::unique_ptr<TrackDesign> _trackDesign;

static void window_track_place_clear_provisional();
static int32_t window_track_place_get_base_z(int32_t x, int32_t y);
static void window_track_place_attempt_placement(
    TrackDesign* td6, int32_t x, int32_t y, int32_t z, int32_t bl, money32* cost, ride_id_t* rideIndex);

static void window_track_place_clear_mini_preview();
static void window_track_place_draw_mini_preview(TrackDesign* td6);
static void window_track_place_draw_mini_preview_track(
    TrackDesign* td6, int32_t pass, CoordsXY origin, CoordsXY& min, CoordsXY& max);
static void window_track_place_draw_mini_preview_maze(
    TrackDesign* td6, int32_t pass, CoordsXY origin, CoordsXY& min, CoordsXY& max);
static LocationXY16 draw_mini_preview_get_pixel_position(int16_t x, int16_t y);
static bool draw_mini_preview_is_pixel_in_bounds(LocationXY16 pixel);
static uint8_t* draw_mini_preview_get_pixel_ptr(LocationXY16 pixel);

/**
 *
 *  rct2: 0x006D182E
 */
static void window_track_place_clear_mini_preview()
{
    // Fill with transparent colour.
    std::fill(_window_track_place_mini_preview.begin(), _window_track_place_mini_preview.end(), PALETTE_INDEX_0);
}

/**
 *
 *  rct2: 0x006CFCA0
 */
rct_window* window_track_place_open(const track_design_file_ref* tdFileRef)
{
    _trackDesign = track_design_open(tdFileRef->path);
    if (_trackDesign == nullptr)
    {
        return nullptr;
    }

    window_close_construction_windows();

    _window_track_place_mini_preview.resize(TRACK_MINI_PREVIEW_SIZE);

    rct_window* w = window_create(ScreenCoordsXY(0, 29), 200, 124, &window_track_place_events, WC_TRACK_DESIGN_PLACE, 0);
    w->widgets = window_track_place_widgets;
    w->enabled_widgets = 1 << WIDX_CLOSE | 1 << WIDX_ROTATE | 1 << WIDX_MIRROR | 1 << WIDX_SELECT_DIFFERENT_DESIGN;
    window_init_scroll_widgets(w);
    tool_set(w, WIDX_PRICE, TOOL_CROSSHAIR);
    input_set_flag(INPUT_FLAG_6, true);
    window_push_others_right(w);
    show_gridlines();
    _window_track_place_last_cost = MONEY32_UNDEFINED;
    _window_track_place_last_x = -1;
    _currentTrackPieceDirection = (2 - get_current_rotation()) & 3;

    window_track_place_clear_mini_preview();
    window_track_place_draw_mini_preview(_trackDesign.get());

    return w;
}

/**
 *
 *  rct2: 0x006D0119
 */
static void window_track_place_close(rct_window* w)
{
    window_track_place_clear_provisional();
    viewport_set_visibility(0);
    map_invalidate_map_selection_tiles();
    gMapSelectFlags &= ~MAP_SELECT_FLAG_ENABLE_CONSTRUCT;
    gMapSelectFlags &= ~MAP_SELECT_FLAG_ENABLE_ARROW;
    hide_gridlines();
    _window_track_place_mini_preview.clear();
    _window_track_place_mini_preview.shrink_to_fit();
    _trackDesign = nullptr;
}

/**
 *
 *  rct2: 0x006CFEAC
 */
static void window_track_place_mouseup(rct_window* w, rct_widgetindex widgetIndex)
{
    switch (widgetIndex)
    {
        case WIDX_CLOSE:
            window_close(w);
            break;
        case WIDX_ROTATE:
            window_track_place_clear_provisional();
            _currentTrackPieceDirection = (_currentTrackPieceDirection + 1) & 3;
            w->Invalidate();
            _window_track_place_last_x = -1;
            window_track_place_draw_mini_preview(_trackDesign.get());
            break;
        case WIDX_MIRROR:
            track_design_mirror(_trackDesign.get());
            _currentTrackPieceDirection = (0 - _currentTrackPieceDirection) & 3;
            w->Invalidate();
            _window_track_place_last_x = -1;
            window_track_place_draw_mini_preview(_trackDesign.get());
            break;
        case WIDX_SELECT_DIFFERENT_DESIGN:
            window_close(w);

            auto intent = Intent(WC_TRACK_DESIGN_LIST);
            intent.putExtra(INTENT_EXTRA_RIDE_TYPE, _window_track_list_item.type);
            intent.putExtra(INTENT_EXTRA_RIDE_ENTRY_INDEX, _window_track_list_item.entry_index);
            context_open_intent(&intent);
            break;
    }
}

/**
 *
 *  rct2: 0x006CFCA0
 */
static void window_track_place_update(rct_window* w)
{
    if (!(input_test_flag(INPUT_FLAG_TOOL_ACTIVE)))
        if (gCurrentToolWidget.window_classification != WC_TRACK_DESIGN_PLACE)
            window_close(w);
}

static GameActionResult::Ptr FindValidTrackDesignPlaceHeight(CoordsXYZ& loc, uint32_t flags)
{
    for (int32_t i = 0; i < 7; i++)
    {
        auto tdAction = TrackDesignAction(CoordsXYZD{ loc.x, loc.y, loc.z, _currentTrackPieceDirection }, *_trackDesign);
        tdAction.SetFlags(flags);
        auto res = GameActions::Query(&tdAction);

        // If successful dont keep trying.
        // If failure due to no money then increasing height only makes problem worse
        if (res->Error == GA_ERROR::OK || res->Error == GA_ERROR::INSUFFICIENT_FUNDS)
        {
            return res;
        }
        // Return the last attempts error up the chain
        if (i == 6)
        {
            return res;
        }
        loc.z += 8;
    }
    return nullptr;
}

/**
 *
 *  rct2: 0x006CFF2D
 */
static void window_track_place_toolupdate(rct_window* w, rct_widgetindex widgetIndex, ScreenCoordsXY screenCoords)
{
    int16_t mapZ;

    map_invalidate_map_selection_tiles();
    gMapSelectFlags &= ~MAP_SELECT_FLAG_ENABLE;
    gMapSelectFlags &= ~MAP_SELECT_FLAG_ENABLE_CONSTRUCT;
    gMapSelectFlags &= ~MAP_SELECT_FLAG_ENABLE_ARROW;

    // Get the tool map position
    CoordsXY mapCoords = sub_68A15E(screenCoords);
    if (mapCoords.x == LOCATION_NULL)
    {
        window_track_place_clear_provisional();
        return;
    }

    // Check if tool map position has changed since last update
    if (mapCoords.x == _window_track_place_last_x && mapCoords.y == _window_track_place_last_y)
    {
        place_virtual_track(
            _trackDesign.get(), PTD_OPERATION_DRAW_OUTLINES, true, GetOrAllocateRide(0), mapCoords.x, mapCoords.y, 0);
        return;
    }

    money32 cost = MONEY32_UNDEFINED;

    // Get base Z position
    mapZ = window_track_place_get_base_z(mapCoords.x, mapCoords.y);
    CoordsXYZ trackLoc = { mapCoords, mapZ };

    if (game_is_not_paused() || gCheatsBuildInPauseMode)
    {
        window_track_place_clear_provisional();
        auto res = FindValidTrackDesignPlaceHeight(trackLoc, GAME_COMMAND_FLAG_NO_SPEND | GAME_COMMAND_FLAG_GHOST);

        if (res->Error == GA_ERROR::OK)
        {
            // Valid location found. Place the ghost at the location.
            ride_id_t rideIndex;
            window_track_place_attempt_placement(
                _trackDesign.get(), trackLoc.x, trackLoc.y, trackLoc.z,
                GAME_COMMAND_FLAG_APPLY | GAME_COMMAND_FLAG_NO_SPEND | GAME_COMMAND_FLAG_GHOST, &cost, &rideIndex);
            if (cost != MONEY32_UNDEFINED)
            {
                _window_track_place_ride_index = rideIndex;
                _window_track_place_last_valid_x = trackLoc.x;
                _window_track_place_last_valid_y = trackLoc.y;
                _window_track_place_last_valid_z = trackLoc.z;
                _window_track_place_last_was_valid = true;
            }
        }
    }

    _window_track_place_last_x = trackLoc.x;
    _window_track_place_last_y = trackLoc.y;
    if (cost != _window_track_place_last_cost)
    {
        _window_track_place_last_cost = cost;
        widget_invalidate(w, WIDX_PRICE);
    }

    place_virtual_track(
        _trackDesign.get(), PTD_OPERATION_DRAW_OUTLINES, true, GetOrAllocateRide(0), trackLoc.x, trackLoc.y, trackLoc.z);
}

/**
 *
 *  rct2: 0x006CFF34
 */
static void window_track_place_tooldown(rct_window* w, rct_widgetindex widgetIndex, ScreenCoordsXY screenCoords)
{
    window_track_place_clear_provisional();
    map_invalidate_map_selection_tiles();
    gMapSelectFlags &= ~MAP_SELECT_FLAG_ENABLE;
    gMapSelectFlags &= ~MAP_SELECT_FLAG_ENABLE_CONSTRUCT;
    gMapSelectFlags &= ~MAP_SELECT_FLAG_ENABLE_ARROW;

    const CoordsXY mapCoords = sub_68A15E(screenCoords);
    if (mapCoords.x == LOCATION_NULL)
        return;

    // Try increasing Z until a feasible placement is found
    int16_t mapZ = window_track_place_get_base_z(mapCoords.x, mapCoords.y);
    CoordsXYZ trackLoc = { mapCoords, mapZ };

    auto res = FindValidTrackDesignPlaceHeight(trackLoc, 0);
    if (res->Error == GA_ERROR::OK)
    {
        auto tdAction = TrackDesignAction({ trackLoc.x, trackLoc.y, trackLoc.z, _currentTrackPieceDirection }, *_trackDesign);
        tdAction.SetCallback([trackLoc](const GameAction*, const TrackDesignActionResult* res) {
            if (res->Error == GA_ERROR::OK)
            {
                auto ride = get_ride(res->rideIndex);
                if (ride != nullptr)
                {
                    window_close_by_class(WC_ERROR);
                    audio_play_sound_at_location(SoundId::PlaceItem, { trackLoc.x, trackLoc.y, trackLoc.z });

                    _currentRideIndex = res->rideIndex;
                    if (track_design_are_entrance_and_exit_placed())
                    {
                        auto intent = Intent(WC_RIDE);
                        intent.putExtra(INTENT_EXTRA_RIDE_ID, res->rideIndex);
                        context_open_intent(&intent);
                        auto w = window_find_by_class(WC_TRACK_DESIGN_PLACE);
                        window_close(w);
                    }
                    else
                    {
                        ride_initialise_construction_window(ride);
                        auto w = window_find_by_class(WC_RIDE_CONSTRUCTION);
                        window_event_mouse_up_call(w, WC_RIDE_CONSTRUCTION__WIDX_ENTRANCE);
                    }
                }
            }
            else
            {
                audio_play_sound_at_location(SoundId::Error, res->Position);
            }
        });
        GameActions::Execute(&tdAction);
        return;
    }

    // Unable to build track
    audio_play_sound_at_location(SoundId::Error, trackLoc);
    context_show_error(res->ErrorTitle, res->ErrorMessage);
}

/**
 *
 *  rct2: 0x006D015C
 */
static void window_track_place_toolabort(rct_window* w, rct_widgetindex widgetIndex)
{
    window_track_place_clear_provisional();
}

/**
 *
 *  rct2: 0x006CFF01
 */
static void window_track_place_unknown14(rct_window* w)
{
    window_track_place_draw_mini_preview(_trackDesign.get());
}

static void window_track_place_invalidate(rct_window* w)
{
    window_track_place_draw_mini_preview(_trackDesign.get());
}

/**
 *
 *  rct2: 0x006D017F
 */
static void window_track_place_clear_provisional()
{
    if (_window_track_place_last_was_valid)
    {
        auto ride = get_ride(_window_track_place_ride_index);
        if (ride != nullptr)
        {
            place_virtual_track(
                _trackDesign.get(), PTD_OPERATION_REMOVE_GHOST, true, ride, _window_track_place_last_valid_x,
                _window_track_place_last_valid_y, _window_track_place_last_valid_z);
            _window_track_place_last_was_valid = false;
        }
    }
}

/**
 *
 *  rct2: 0x006D17C6
 */
static int32_t window_track_place_get_base_z(int32_t x, int32_t y)
{
    uint32_t z;

    auto surfaceElement = map_get_surface_element_at(x >> 5, y >> 5);
    if (surfaceElement == nullptr)
        return 0;
    z = surfaceElement->base_height * 8;

    // Increase Z above slope
    if (surfaceElement->GetSlope() & TILE_ELEMENT_SLOPE_ALL_CORNERS_UP)
    {
        z += 16;

        // Increase Z above double slope
        if (surfaceElement->GetSlope() & TILE_ELEMENT_SLOPE_DOUBLE_HEIGHT)
            z += 16;
    }

    // Increase Z above water
    if (surfaceElement->GetWaterHeight() > 0)
        z = std::max(z, surfaceElement->GetWaterHeight() << 4);

    return z + place_virtual_track(_trackDesign.get(), PTD_OPERATION_GET_PLACE_Z, true, GetOrAllocateRide(0), x, y, z);
}

static void window_track_place_attempt_placement(
    TrackDesign* td6, int32_t x, int32_t y, int32_t z, int32_t bl, money32* cost, ride_id_t* rideIndex)
{
    auto tdAction = TrackDesignAction({ x, y, z, _currentTrackPieceDirection }, *_trackDesign);
    tdAction.SetFlags(bl);
    auto res = (bl & GAME_COMMAND_FLAG_APPLY) ? GameActions::Execute(&tdAction) : GameActions::Query(&tdAction);

    if (res->Error != GA_ERROR::OK)
    {
        *cost = MONEY32_UNDEFINED;
    }
    else
    {
        *cost = res->Cost;
    }
    *rideIndex = dynamic_cast<TrackDesignActionResult*>(res.get())->rideIndex;
}

/**
 *
 *  rct2: 0x006CFD9D
 */
static void window_track_place_paint(rct_window* w, rct_drawpixelinfo* dpi)
{
    set_format_arg(0, char*, _trackDesign->name.c_str());
    window_draw_widgets(w, dpi);

    // Draw mini tile preview
    rct_drawpixelinfo clippedDpi;
    if (clip_drawpixelinfo(&clippedDpi, dpi, w->x + 4, w->y + 18, 168, 78))
    {
        rct_g1_element g1temp = {};
        g1temp.offset = _window_track_place_mini_preview.data();
        g1temp.width = TRACK_MINI_PREVIEW_WIDTH;
        g1temp.height = TRACK_MINI_PREVIEW_HEIGHT;
        gfx_set_g1_element(SPR_TEMP, &g1temp);
        gfx_draw_sprite(&clippedDpi, SPR_TEMP | SPRITE_ID_PALETTE_COLOUR_1(NOT_TRANSLUCENT(w->colours[0])), 0, 0, 0);
    }

    // Price
    if (_window_track_place_last_cost != MONEY32_UNDEFINED && !(gParkFlags & PARK_FLAGS_NO_MONEY))
    {
        gfx_draw_string_centred(dpi, STR_COST_LABEL, w->x + 88, w->y + 94, COLOUR_BLACK, &_window_track_place_last_cost);
    }
}

/**
 *
 *  rct2: 0x006D1845
 */
static void window_track_place_draw_mini_preview(TrackDesign* td6)
{
    window_track_place_clear_mini_preview();

    // First pass is used to determine the width and height of the image so it can centre it
    CoordsXY min = { 0, 0 };
    CoordsXY max = { 0, 0 };
    for (int32_t pass = 0; pass < 2; pass++)
    {
        CoordsXY origin = { 0, 0 };
        if (pass == 1)
        {
            origin.x -= ((max.x + min.x) >> 6) * 32;
            origin.y -= ((max.y + min.y) >> 6) * 32;
        }

        if (td6->type == RIDE_TYPE_MAZE)
        {
            window_track_place_draw_mini_preview_maze(td6, pass, origin, min, max);
        }
        else
        {
            window_track_place_draw_mini_preview_track(td6, pass, origin, min, max);
        }
    }
}

static void window_track_place_draw_mini_preview_track(
    TrackDesign* td6, int32_t pass, CoordsXY origin, CoordsXY& min, CoordsXY& max)
{
    uint8_t rotation = (_currentTrackPieceDirection + get_current_rotation()) & 3;

    const rct_preview_track** trackBlockArray = (ride_type_has_flag(td6->type, RIDE_TYPE_FLAG_HAS_TRACK)) ? TrackBlocks
                                                                                                          : FlatRideTrackBlocks;
    for (const auto& trackElement : td6->track_elements)
    {
        int32_t trackType = trackElement.type;
        if (trackType == TRACK_ELEM_INVERTED_90_DEG_UP_TO_FLAT_QUARTER_LOOP_ALIAS)
        {
            trackType = TRACK_ELEM_MULTIDIM_INVERTED_90_DEG_UP_TO_FLAT_QUARTER_LOOP;
        }

        // Follow a single track piece shape
        const rct_preview_track* trackBlock = trackBlockArray[trackType];
        while (trackBlock->index != 255)
        {
            auto rotatedAndOffsetTrackBlock = origin + CoordsXY{ trackBlock->x, trackBlock->y }.Rotate(rotation);

            if (pass == 0)
            {
                min.x = std::min(min.x, rotatedAndOffsetTrackBlock.x);
                max.x = std::max(max.x, rotatedAndOffsetTrackBlock.x);
                min.y = std::min(min.y, rotatedAndOffsetTrackBlock.y);
                max.y = std::max(max.y, rotatedAndOffsetTrackBlock.y);
            }
            else
            {
                LocationXY16 pixelPosition = draw_mini_preview_get_pixel_position(
                    rotatedAndOffsetTrackBlock.x, rotatedAndOffsetTrackBlock.y);
                if (draw_mini_preview_is_pixel_in_bounds(pixelPosition))
                {
                    uint8_t* pixel = draw_mini_preview_get_pixel_ptr(pixelPosition);

                    auto bits = trackBlock->var_08.Rotate(rotation & 3).GetBaseQuarterOccupied();

                    // Station track is a lighter colour
                    uint8_t colour = (TrackSequenceProperties[trackType][0] & TRACK_SEQUENCE_FLAG_ORIGIN)
                        ? _PaletteIndexColourStation
                        : _PaletteIndexColourTrack;

                    for (int32_t i = 0; i < 4; i++)
                    {
                        if (bits & 1)
                            pixel[338 + i] = colour; // x + 2, y + 2
                        if (bits & 2)
                            pixel[168 + i] = colour; //        y + 1
                        if (bits & 4)
                            pixel[2 + i] = colour; // x + 2
                        if (bits & 8)
                            pixel[172 + i] = colour; // x + 4, y + 1
                    }
                }
            }
            trackBlock++;
        }

        // Change rotation and next position based on track curvature
        rotation &= 3;
        const rct_track_coordinates* track_coordinate = &TrackCoordinates[trackType];

        trackType *= 10;
        auto rotatedAndOfffsetTrack = origin + CoordsXY{ track_coordinate->x, track_coordinate->y }.Rotate(rotation);
        rotation += track_coordinate->rotation_end - track_coordinate->rotation_begin;
        rotation &= 3;
        if (track_coordinate->rotation_end & 4)
        {
            rotation |= 4;
        }
        if (!(rotation & 4))
        {
            origin = rotatedAndOfffsetTrack + CoordsDirectionDelta[rotation];
        }
    }

    // Draw entrance and exit preview.
    for (const auto& entrance : td6->entrance_elements)
    {
        auto rotatedAndOffsetEntrance = origin + CoordsXY{ entrance.x, entrance.y }.Rotate(rotation);

        if (pass == 0)
        {
            min.x = std::min(min.x, rotatedAndOffsetEntrance.x);
            max.x = std::max(max.x, rotatedAndOffsetEntrance.x);
            min.y = std::min(min.y, rotatedAndOffsetEntrance.y);
            max.y = std::max(max.y, rotatedAndOffsetEntrance.y);
        }
        else
        {
            LocationXY16 pixelPosition = draw_mini_preview_get_pixel_position(
                rotatedAndOffsetEntrance.x, rotatedAndOffsetEntrance.y);
            if (draw_mini_preview_is_pixel_in_bounds(pixelPosition))
            {
                uint8_t* pixel = draw_mini_preview_get_pixel_ptr(pixelPosition);
                uint8_t colour = entrance.isExit ? _PaletteIndexColourExit : _PaletteIndexColourEntrance;
                for (int32_t i = 0; i < 4; i++)
                {
                    pixel[338 + i] = colour; // x + 2, y + 2
                    pixel[168 + i] = colour; //        y + 1
                    pixel[2 + i] = colour;   // x + 2
                    pixel[172 + i] = colour; // x + 4, y + 1
                }
            }
        }
    }
}

static void window_track_place_draw_mini_preview_maze(
    TrackDesign* td6, int32_t pass, CoordsXY origin, CoordsXY& min, CoordsXY& max)
{
    uint8_t rotation = (_currentTrackPieceDirection + get_current_rotation()) & 3;
    for (const auto& mazeElement : td6->maze_elements)
    {
        auto rotatedMazeCoords = origin + CoordsXY{ mazeElement.x * 32, mazeElement.y * 32 }.Rotate(rotation);

        if (pass == 0)
        {
            min.x = std::min(min.x, rotatedMazeCoords.x);
            max.x = std::max(max.x, rotatedMazeCoords.x);
            min.y = std::min(min.y, rotatedMazeCoords.y);
            max.y = std::max(max.y, rotatedMazeCoords.y);
        }
        else
        {
            LocationXY16 pixelPosition = draw_mini_preview_get_pixel_position(rotatedMazeCoords.x, rotatedMazeCoords.y);
            if (draw_mini_preview_is_pixel_in_bounds(pixelPosition))
            {
                uint8_t* pixel = draw_mini_preview_get_pixel_ptr(pixelPosition);

                uint8_t colour = _PaletteIndexColourTrack;

                // Draw entrance and exit with different colours.
                if (mazeElement.type == MAZE_ELEMENT_TYPE_ENTRANCE)
                    colour = _PaletteIndexColourEntrance;
                else if (mazeElement.type == MAZE_ELEMENT_TYPE_EXIT)
                    colour = _PaletteIndexColourExit;

                for (int32_t i = 0; i < 4; i++)
                {
                    pixel[338 + i] = colour; // x + 2, y + 2
                    pixel[168 + i] = colour; //        y + 1
                    pixel[2 + i] = colour;   // x + 2
                    pixel[172 + i] = colour; // x + 4, y + 1
                }
            }
        }
    }
}

static LocationXY16 draw_mini_preview_get_pixel_position(int16_t x, int16_t y)
{
    return { (int16_t)(80 + ((y / 32) - (x / 32)) * 4), (int16_t)(38 + ((y / 32) + (x / 32)) * 2) };
}

static bool draw_mini_preview_is_pixel_in_bounds(LocationXY16 pixel)
{
    return pixel.x >= 0 && pixel.y >= 0 && pixel.x <= 160 && pixel.y <= 75;
}

static uint8_t* draw_mini_preview_get_pixel_ptr(LocationXY16 pixel)
{
    return &_window_track_place_mini_preview[pixel.y * TRACK_MINI_PREVIEW_WIDTH + pixel.x];
}
