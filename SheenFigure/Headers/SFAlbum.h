/*
 * Copyright (C) 2016 Muhammad Tayyab Akram
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef _SF_ALBUM_H
#define _SF_ALBUM_H

#include "SFTypes.h"

struct _SFAlbum;
typedef struct _SFAlbum SFAlbum;
/**
 * The type used to represent a glyph set.
 */
typedef SFAlbum *SFAlbumRef;

SFAlbumRef SFAlbumCreate(void);

void SFAlbumClear(SFAlbumRef album);

/**
 * Provides the range of analysed text.
 * @param album
 *      The album whose text range you want to obtain.
 * @return
 *      The range of input text analysed by the shaping process.
 */
SFRange SFAlbumGetTextRange(SFAlbumRef album);

/**
 * Provides the number of produced glyphs.
 * @param album
 *      The album whose number of glyphs you want to obtain.
 * @return
 *      The number of glyphs kept by the album.
 */
SFUInteger SFAlbumGetGlyphCount(SFAlbumRef album);

/**
 * Provides an array of glyphs corresponding to the input text.
 * @param album
 *      The album whose glyphs you want to obtain.
 * @return
 *      An array of glyphs produced as part of shaping process.
 */
SFGlyphID *SFAlbumGetGlyphIDs(SFAlbumRef album);

/**
 * Provides an array of glyph positions in font units where each glyph is positioned with respect to
 * zero origin.
 * @param album
 *      The album whose glyph positions you want to obtain.
 * @return
 *      An array of glyph positions produced as part of shaping process.
 */
SFPoint *SFAlbumGetGlyphPositions(SFAlbumRef album);

/**
 * Provides an array of glyph advances in font units.
 * @param album
 *      The album whose glyph advances you want to obtain.
 * @return
 *      An array of glyph advances produced as part of shaping process.
 */
SFInteger *SFAlbumGetGlyphAdvances(SFAlbumRef album);

/**
 * Provides an array, mapping each character to corresponding range of glyph/s.
 * @param album
 *      The album whose charater to glyph map you want to obtain.
 * @return
 *      An array of ranges mapping characters to glyphs.
 */
SFRange *SFAlbumGetCharacterToGlyphMap(SFAlbumRef album);

SFAlbumRef SFAlbumRetain(SFAlbumRef album);
void SFAlbumRelease(SFAlbumRef album);

#endif