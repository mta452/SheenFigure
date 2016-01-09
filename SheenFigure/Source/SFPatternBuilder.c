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

#include <SFConfig.h>
#include <SFFeatureTag.h>
#include <SFLanguageTag.h>
#include <SFScriptTag.h>
#include <SFTypes.h>

#include <stddef.h>
#include <stdlib.h>

#include "SFAssert.h"
#include "SFList.h"
#include "SFPattern.h"
#include "SFPatternBuilder.h"

SF_INTERNAL void SFPatternBuilderInitialize(SFPatternBuilderRef builder, SFPatternRef pattern)
{
    /* Pattern must NOT be null. */
    SFAssert(pattern != NULL);

    /* Initialize the builder. */
    builder->_pattern = pattern;
    builder->_font = NULL;
    builder->_featureKind = 0;
    builder->_gsubUnitCount = 0;
    builder->_gposUnitCount = 0;
    builder->_featureIndex = 0;
    builder->_requiredTraits = 0;
    builder->_scriptTag = 0;
    builder->_languageTag = 0;
    builder->_canBuild = SFTrue;

    SFListInitialize(&builder->_featureTags, sizeof(SFFeatureTag));
    SFListSetCapacity(&builder->_featureTags, 24);

    SFListInitialize(&builder->_featureUnits, sizeof(SFFeatureUnit));
    SFListSetCapacity(&builder->_featureUnits, 24);

    SFListInitialize(&builder->_lookupIndexes, sizeof(SFUInt16));
    SFListSetCapacity(&builder->_lookupIndexes, 32);
}

SF_INTERNAL void SFPatternBuilderFinalize(SFPatternBuilderRef builder)
{
    /* The pattern MUST be built before finalizing the builder. */
    SFAssert(builder->_canBuild == SFFalse);

    SFListFinalize(&builder->_lookupIndexes);
}

SF_INTERNAL void SFPatternBuilderSetFont(SFPatternBuilderRef builder, SFFontRef font)
{
    /* Font must NOT be null. */
    SFAssert(font != NULL);

    builder->_font = font;
}

SF_INTERNAL void SFPatternBuilderSetScript(SFPatternBuilderRef builder, SFScriptTag scriptTag)
{
    builder->_scriptTag = scriptTag;
}

SF_INTERNAL void SFPatternBuilderSetLanguage(SFPatternBuilderRef builder, SFLanguageTag languageTag)
{
    builder->_languageTag = languageTag;
}

SF_INTERNAL void SFPatternBuilderBeginFeatures(SFPatternBuilderRef builder, SFFeatureKind featureKind)
{
    /* One kind of features must be ended before beginning a new one. */
    SFAssert(builder->_featureKind == 0);

    builder->_featureKind = featureKind;
}

SF_INTERNAL void SFPatternBuilderAddFeature(SFPatternBuilderRef builder, SFFeatureTag featureTag, SFGlyphTraits requiredTraits)
{
    /* The kind of features must be specified before adding them. */
    SFAssert(builder->_featureKind != 0);
    /* Only unique features can be added. */
    SFAssert(!SFListContainsItem(&builder->_featureTags, &featureTag));

    /* Add the feature in the list. */
    SFListAdd(&builder->_featureTags, featureTag);
    /* Insert the required traits of the feature. */
    builder->_requiredTraits |= requiredTraits;
}

SF_INTERNAL void SFPatternBuilderAddLookup(SFPatternBuilderRef builder, SFUInt16 lookupIndex)
{
    /* A feature MUST be available before adding lookups. */
    SFAssert((builder->_featureTags.count - builder->_featureIndex) > 0);

    /* Add only unique lookup indexes. */
    if (!SFListContainsItem(&builder->_lookupIndexes, &lookupIndex)) {
        SFListAdd(&builder->_lookupIndexes, lookupIndex);
    }
}

static int _SFLookupIndexComparison(const void *item1, const void *item2)
{
    SFUInteger *ref1 = (SFUInteger *)item1;
    SFUInteger *ref2 = (SFUInteger *)item2;

    return (int)(*ref1 - *ref2);
}

SF_INTERNAL void SFPatternBuilderMakeFeatureUnit(SFPatternBuilderRef builder)
{
    SFFeatureUnit featureUnit;

    /* At least one feature MUST be available before making a feature unit. */
    SFAssert((builder->_featureTags.count - builder->_featureIndex) > 0);

    /* Sort all lookup indexes. */
    SFListSort(&builder->_lookupIndexes, 0, builder->_lookupIndexes.count, _SFLookupIndexComparison);
    /* Set lookup indexes in current feature unit. */
    SFListFinalizeKeepingArray(&builder->_lookupIndexes, &featureUnit.lookupIndexes.items, &featureUnit.lookupIndexes.count);
    /* Set covered range of feature unit. */
    featureUnit.coveredRange.start = builder->_featureIndex;
    featureUnit.coveredRange.length = builder->_featureTags.count - builder->_featureIndex;
    featureUnit.requiredTraits = builder->_requiredTraits;

    /* Add the feature unit in the list. */
    SFListAdd(&builder->_featureUnits, featureUnit);

    /* Increase feature unit count. */
    switch (builder->_featureKind) {
    case SFFeatureKindSubstitution:
        builder->_gsubUnitCount++;
        break;

    case SFFeatureKindPositioning:
        builder->_gposUnitCount++;
        break;
    }

    /* Increase feature index. */
    builder->_featureIndex += featureUnit.coveredRange.length;

    /* Initialize lookup indexes array. */
    SFListInitialize(&builder->_lookupIndexes, sizeof(SFUInt16));
    SFListSetCapacity(&builder->_lookupIndexes, 32);
    /* Reset required traits. */
    builder->_requiredTraits = SFGlyphTraitNone;
}

SF_INTERNAL void SFPatternBuilderEndFeatures(SFPatternBuilderRef builder)
{
    /* The features of specified kind MUST be begun before ending them. */
    SFAssert(builder->_featureKind != 0);
    /* There should be NO pending feature unit. */
    SFAssert(builder->_featureTags.count == builder->_featureIndex);

    builder->_featureKind = 0;
}

SF_INTERNAL void SFPatternBuilderBuild(SFPatternBuilderRef builder)
{
    SFPatternRef pattern = builder->_pattern;
    SFUInteger unitCount;

    /* All features MUST be ended before building the pattern. */
    SFAssert(builder->_featureKind == 0);

    /* Initialize the pattern. */
    pattern->font = SFFontRetain(builder->_font);
    pattern->featureUnits.gsub = builder->_gsubUnitCount;
    pattern->featureUnits.gpos = builder->_gposUnitCount;
    pattern->scriptTag = builder->_scriptTag;
    pattern->languageTag = builder->_languageTag;

    SFListFinalizeKeepingArray(&builder->_featureTags, &pattern->featureTags.items, &pattern->featureTags.count);
    SFListFinalizeKeepingArray(&builder->_featureUnits, &pattern->featureUnits.items, &unitCount);

    builder->_canBuild = SFFalse;
}