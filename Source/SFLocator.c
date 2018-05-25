/*
 * Copyright (C) 2015-2018 Muhammad Tayyab Akram
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

#include <SFConfig.h>
#include <stddef.h>

#include "SFAssert.h"
#include "SFAlbum.h"
#include "SFBase.h"
#include "SFGDEF.h"
#include "SFOpenType.h"
#include "SFLocator.h"

SF_INTERNAL void SFLocatorInitialize(SFLocatorRef locator, SFAlbumRef album, SFData gdef)
{
    /* Album must NOT be null. */
    SFAssert(album != NULL);

    locator->_album = album;
    locator->_markAttachClassDef = NULL;
    locator->_markGlyphSetsDef = NULL;
    locator->_markFilteringCoverage = NULL;
    locator->_version = SFInvalidIndex;
    locator->_startIndex = 0;
    locator->_limitIndex = 0;
    locator->_stateIndex = 0;
    locator->index = SFInvalidIndex;
    locator->_ignoreMask.full = 0;
    locator->lookupFlag = 0;

    if (gdef) {
        locator->_markAttachClassDef = SFGDEF_MarkAttachClassDefTable(gdef);

        if (SFGDEF_Version(gdef) == 0x00010002) {
            locator->_markGlyphSetsDef = SFGDEF_MarkGlyphSetsDefTable(gdef);
        }
    }
}

SF_INTERNAL void SFLocatorReserveGlyphs(SFLocatorRef locator, SFUInteger glyphCount)
{
    /* The album version MUST be same. */
    SFAssert(locator->_version == locator->_album->_version);

    SFAlbumReserveGlyphs(locator->_album, locator->_stateIndex, glyphCount);

    locator->_version = locator->_album->_version;
    locator->_limitIndex += glyphCount;
}

SF_INTERNAL void SFLocatorSetFeatureMask(SFLocatorRef locator, SFUInt16 featureMask)
{
    locator->_ignoreMask.section.feature = _SFAlbumGetAntiFeatureMask(featureMask);
}

SF_INTERNAL void SFLocatorSetLookupFlag(SFLocatorRef locator, SFLookupFlag lookupFlag)
{
    SFGlyphTraits ignoreTraits = SFGlyphTraitNone;

    if (lookupFlag & SFLookupFlagIgnoreBaseGlyphs) {
        ignoreTraits |= SFGlyphTraitBase;
    }

    if (lookupFlag & SFLookupFlagIgnoreLigatures) {
        ignoreTraits |= SFGlyphTraitLigature;
    }

    if (lookupFlag & SFLookupFlagIgnoreMarks) {
        ignoreTraits |= SFGlyphTraitMark;
    }

    ignoreTraits |= SFGlyphTraitPlaceholder;

    locator->lookupFlag = lookupFlag;
    locator->_ignoreMask.section.traits = ignoreTraits;
}

SF_INTERNAL void SFLocatorSetMarkFilteringSet(SFLocatorRef locator, SFUInt16 markFilteringSet)
{
    SFData markGlyphSetsDef = locator->_markGlyphSetsDef;

    locator->_markFilteringCoverage = NULL;

    if (markGlyphSetsDef) {
        SFUInt16 format = SFMarkGlyphSets_Format(markGlyphSetsDef);
        switch (format) {
            case 1: {
                SFUInt16 markSetCount = SFMarkGlyphSets_MarkSetCount(markGlyphSetsDef);

                if (markFilteringSet < markSetCount) {
                    locator->_markFilteringCoverage = SFMarkGlyphSets_CoverageTable(markGlyphSetsDef, markFilteringSet);
                }
                break;
            }
        }
    }
}

SF_INTERNAL void SFLocatorReset(SFLocatorRef locator, SFUInteger index, SFUInteger count)
{
    /* The index must be valid and there should be no integer overflow. */
    SFAssert(index <= locator->_album->glyphCount && index <= (index + count));

    locator->_version = locator->_album->_version;
    locator->_startIndex = index;
    locator->_limitIndex = index + count;
    locator->_stateIndex = index;
    locator->index = SFInvalidIndex;
}

static SFBoolean _SFIsIgnoredGlyph(SFLocatorRef locator, SFUInteger index) {
    SFAlbumRef album = locator->_album;
    SFLookupFlag lookupFlag = locator->lookupFlag;
    SFGlyphMask glyphMask = _SFAlbumGetGlyphMask(album, index);

    if (locator->_ignoreMask.full & glyphMask.full) {
        return SFTrue;
    }

    if (glyphMask.section.traits & SFGlyphTraitMark) {
        if (lookupFlag & SFLookupFlagUseMarkFilteringSet) {
            SFData markFilteringCoverage = locator->_markFilteringCoverage;

            if (markFilteringCoverage) {
                SFGlyphID glyph = SFAlbumGetGlyph(album, index);
                SFUInteger coverageIndex = SFOpenTypeSearchCoverageIndex(markFilteringCoverage, glyph);

                if (coverageIndex == SFInvalidIndex) {
                    return SFTrue;
                }
            }
        }

        if (lookupFlag & SFLookupFlagMarkAttachmentType) {
            SFData markAttachClassDef = locator->_markAttachClassDef;

            if (markAttachClassDef) {
                SFGlyphID glyph = SFAlbumGetGlyph(album, index);
                SFUInt16 glyphClass = SFOpenTypeSearchGlyphClass(markAttachClassDef, glyph);

                if (glyphClass != (lookupFlag >> 8)) {
                    return SFTrue;
                }
            }
        }
    }

    return SFFalse;
}

SF_INTERNAL SFBoolean SFLocatorMoveNext(SFLocatorRef locator)
{
    /* The state of locator must be valid. */
    SFAssert(locator->_stateIndex >= locator->_startIndex && locator->_stateIndex <= locator->_limitIndex);
    /* The album version MUST be same. */
    SFAssert(locator->_version == locator->_album->_version);

    while (locator->_stateIndex < locator->_limitIndex) {
        SFUInteger index = locator->_stateIndex++;

        if (!_SFIsIgnoredGlyph(locator, index)) {
            locator->index = index;
            return SFTrue;
        }
    }

    locator->index = SFInvalidIndex;
    return SFFalse;
}

SF_INTERNAL SFBoolean SFLocatorMovePrevious(SFLocatorRef locator)
{
    /* The state of locator must be valid. */
    SFAssert(locator->_stateIndex >= locator->_startIndex && locator->_stateIndex <= locator->_limitIndex);
    /* The album version MUST be same. */
    SFAssert(locator->_version == locator->_album->_version);

    while (locator->_stateIndex > locator->_startIndex) {
        SFUInteger index = --locator->_stateIndex;

        if (!_SFIsIgnoredGlyph(locator, index)) {
            locator->index = index;
            return SFTrue;
        }
    }

    locator->index = SFInvalidIndex;
    return SFFalse;
}

SF_INTERNAL SFBoolean SFLocatorSkip(SFLocatorRef locator, SFUInteger count)
{
    SFUInteger skip;

    for (skip = count; skip != 0; skip--) {
        if (SFLocatorMoveNext(locator)) {
            continue;
        }

        return SFFalse;
    }

    return SFTrue;
}

SF_INTERNAL void SFLocatorJumpTo(SFLocatorRef locator, SFUInteger index)
{
    /*
     * The index must be valid.
     *
     * NOTE:
     *      - It is legal to jump to limit index so that MoveNext method returns SFFalse thereafter.
     *      - Similarly, it is legal to jump to start index so that MovePrevious method returns
     *        SFFalse thereafter.
     */
    SFAssert(index >= locator->_startIndex && index <= locator->_limitIndex);
    /* The album version MUST be same. */
    SFAssert(locator->_version == locator->_album->_version);

    locator->_stateIndex = index;
}

SF_INTERNAL SFUInteger SFLocatorGetAfter(SFLocatorRef locator, SFUInteger index)
{
    /* The index must be valid. */
    SFAssert(index >= locator->_startIndex && index <= locator->_limitIndex);
    /* The album version MUST be same. */
    SFAssert(locator->_version == locator->_album->_version);

    while (++index < locator->_limitIndex) {
        if (!_SFIsIgnoredGlyph(locator, index)) {
            return index;
        }
    }

    return SFInvalidIndex;
}

SF_INTERNAL SFUInteger SFLocatorGetBefore(SFLocatorRef locator, SFUInteger index)
{
    /* The index must be valid. */
    SFAssert(index >= locator->_startIndex && index <= locator->_limitIndex);
    /* The album version MUST be same. */
    SFAssert(locator->_version == locator->_album->_version);

    while (index-- > locator->_startIndex) {
        if (!_SFIsIgnoredGlyph(locator, index)) {
            return index;
        }
    }

    return SFInvalidIndex;
}

SFUInteger SFLocatorGetPrecedingBaseIndex(SFLocatorRef locator)
{
    SFGlyphTraits ignoreTraits = locator->_ignoreMask.section.traits;
    SFUInteger baseIndex;

    /*
     * Ignore marks only.
     *
     * NOTE:
     *      Multiple substitution sequence is also ignored to make sure that mark aligns with first
     *      corresponding glyph of a base.
     */
    locator->_ignoreMask.section.traits = SFGlyphTraitPlaceholder | SFGlyphTraitMark | SFGlyphTraitSequence;

    /* Get preeding glyph. */
    baseIndex = SFLocatorGetBefore(locator, locator->index);

    /* Restore ignore traits. */
    locator->_ignoreMask.section.traits = ignoreTraits;

    return baseIndex;
}

SF_INTERNAL SFUInteger SFLocatorGetPrecedingLigatureIndex(SFLocatorRef locator, SFUInteger *outComponent)
{
    SFAlbumRef album = locator->_album;
    SFGlyphTraits ignoreTraits = locator->_ignoreMask.section.traits;
    SFUInteger ligIndex;

    /* Initialize component counter. */
    *outComponent = 0;

    /* Ignore marks only. */
    locator->_ignoreMask.section.traits = SFGlyphTraitPlaceholder | SFLookupFlagIgnoreMarks;

    /* Get preeding glyph. */
    ligIndex = SFLocatorGetBefore(locator, locator->index);

    if (ligIndex != SFInvalidIndex) {
        SFUInteger nextIndex;

        /*
         * REMARKS:
         *      The glyphs acting as components of a ligature are not removed from the album, but
         *      their trait is set to SFGlyphTraitPlaceholder.
         *
         * PROCESS:
         *      1) Start loop from ligature index to input index.
         *      2) If a placeholder is found, it is considered a component of the ligature.
         *      3) Increase component counter for each placeholder.
         */
        for (nextIndex = ligIndex + 1; nextIndex < locator->index; nextIndex++) {
            if (SFAlbumGetAllTraits(album, nextIndex) & SFGlyphTraitPlaceholder) {
                (*outComponent)++;
            }
        }
    }

    /* Restore ignore traits. */
    locator->_ignoreMask.section.traits = ignoreTraits;

    return ligIndex;
}

SF_INTERNAL SFUInteger SFLocatorGetPrecedingMarkIndex(SFLocatorRef locator)
{
    SFGlyphTraits ignoreTraits = locator->_ignoreMask.section.traits;
    SFUInteger markIndex;

    /*
     * Ignore marks specified by lookup flag only.
     *
     * NOTE:
     *      Placeholders are also considered to make sure that marks belong to the same component of
     *      a ligature.
     */
    locator->_ignoreMask.section.traits = SFGlyphTraitNone;

    /* Get preeding glyph. */
    markIndex = SFLocatorGetBefore(locator, locator->index);

    /* Fix mark index in case of placeholder. */
    if (markIndex != SFInvalidIndex) {
        SFAlbumRef album = locator->_album;
        SFGlyphTraits traits;

        traits = SFAlbumGetAllTraits(album, markIndex);
        if (traits & SFGlyphTraitPlaceholder) {
            markIndex = SFInvalidIndex;
        }
    }

    /* Restore ignore traits. */
    locator->_ignoreMask.section.traits = ignoreTraits;

    return markIndex;
}

SF_INTERNAL void SFLocatorTakeState(SFLocatorRef locator, SFLocatorRef sibling) {
    /* Both of the locators MUST belong to the same album. */
    SFAssert(locator->_album == sibling->_album);
    /* The state of sibling must be valid. */
    SFAssert(sibling->_stateIndex <= locator->_limitIndex);

    locator->_stateIndex = sibling->_stateIndex;
    locator->_version = sibling->_version;
}
