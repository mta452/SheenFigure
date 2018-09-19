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
#include <stdlib.h>
#include <string.h>

#include "SFBase.h"
#include "SFCommon.h"
#include "SFFont.h"
#include "SFPatternBuilder.h"
#include "SFPattern.h"
#include "SFUnifiedEngine.h"
#include "SFScheme.h"

static SFData _SFSearchScriptTable(SFData scriptListTable, SFTag scriptTag)
{
    SFData scriptTable = NULL;
    SFUInt16 scriptCount;
    SFUInt16 index;

    scriptCount = SFScriptList_ScriptCount(scriptListTable);

    for (index = 0; index < scriptCount; index++) {
        SFData scriptRecord = SFScriptList_ScriptRecord(scriptListTable, index);
        SFTag scriptRecTag = SFScriptRecord_ScriptTag(scriptRecord);
        SFOffset scriptOffset;

        if (scriptRecTag == scriptTag) {
            scriptOffset = SFScriptRecord_ScriptOffset(scriptRecord);
            scriptTable = SFData_Subdata(scriptListTable, scriptOffset);
            break;
        }
    }

    return scriptTable;
}

static SFData _SFSearchLangSysTable(SFData scriptTable, SFTag languageTag)
{
    SFData langSysTable = NULL;

    if (languageTag == SFTagMake('d', 'f', 'l', 't')) {
        SFOffset langSysOffset = SFScript_DefaultLangSysOffset(scriptTable);

        if (langSysOffset) {
            langSysTable = SFData_Subdata(scriptTable, langSysOffset);
        }
    } else {
        SFUInt16 langSysCount = SFScript_LangSysCount(scriptTable);
        SFUInt16 index;

        for (index = 0; index < langSysCount; index++) {
            SFData langSysRecord = SFScript_LangSysRecord(scriptTable, index);
            SFTag langSysTag = SFLangSysRecord_LangSysTag(langSysRecord);
            SFOffset langSysOffset;

            if (langSysTag == languageTag) {
                langSysOffset = SFLangSysRecord_LangSysOffset(langSysRecord);
                langSysTable = SFData_Subdata(scriptTable, langSysOffset);
                break;
            }
        }
    }

    return langSysTable;
}

static SFData _SFSearchFeatureTable(SFData langSysTable, SFData featureListTable, SFTag featureTag)
{
    SFData featureTable = NULL;
    SFUInt16 featureCount;
    SFUInt16 index;

    featureCount = SFLangSys_FeatureCount(langSysTable);

    for (index = 0; index < featureCount; index++) {
        SFUInt16 featureIndex = SFLangSys_FeatureIndex(langSysTable, index);
        SFData featureRecord = SFFeatureList_FeatureRecord(featureListTable, featureIndex);
        SFTag featureRecTag = SFFeatureRecord_FeatureTag(featureRecord);
        SFOffset featureOffset;

        if (featureRecTag == featureTag) {
            featureOffset = SFFeatureRecord_FeatureOffset(featureRecord);
            featureTable = SFData_Subdata(featureListTable, featureOffset);
            break;
        }
    }

    return featureTable;
}

static void _SFAddFeatureLookups(SFPatternBuilderRef patternBuilder, SFData featureTable)
{
    SFUInt16 lookupCount = SFFeature_LookupCount(featureTable);
    SFUInt16 index;

    for (index = 0; index < lookupCount; index++) {
        SFUInt16 lookupListIndex = SFFeature_LookupListIndex(featureTable, index);
        SFPatternBuilderAddLookup(patternBuilder, lookupListIndex);
    }
}

static void _SFAddFeatureUnit(SFPatternBuilderRef patternBuilder,
    SFData langSysTable, SFData featureListTable,
    SFFeatureInfo *featureInfos, SFUInteger featureCount)
{
    SFBoolean exists = SFFalse;
    SFUInteger index;

    for (index = 0; index < featureCount; index++) {
        SFFeatureInfoRef featureInfo = &featureInfos[index];

        /* Skip those features which are off by default. */
        if (featureInfo->status != OFF_BY_DEFAULT) {
            SFData featureTable = _SFSearchFeatureTable(langSysTable, featureListTable, featureInfo->tag);

            /* Add the feature, if it exists in the language. */
            if (featureTable) {
                SFPatternBuilderAddFeature(patternBuilder, featureInfo->tag, featureInfo->mask);
                _SFAddFeatureLookups(patternBuilder, featureTable);

                exists = SFTrue;
            }
        }
    }

    if (exists) {
        SFPatternBuilderMakeFeatureUnit(patternBuilder);
    }
}

static void _SFAddKnownFeatures(SFPatternBuilderRef patternBuilder,
    SFData langSysTable, SFData featureListTable,
    SFFeatureInfo *featureInfos, SFUInteger featureCount)
{
    SFUInteger index = 0;

    while (index < featureCount) {
        SFFeatureInfoRef featureInfo = &featureInfos[index];
        SFUInteger unitLength = 1;

        if (featureInfo->execution == SIMULTANEOUS) {
            SFUInt8 group = featureInfo->group;
            SFUInteger next;

            /* Find out the number of features that need to be applied simultaneously. */
            for (next = index + 1; next < featureCount; next++) {
                if (featureInfos[next].group != group
                    || featureInfos[next].execution != SIMULTANEOUS)
                {
                    break;
                }
            }

            unitLength = next - index;
        }

        _SFAddFeatureUnit(patternBuilder, langSysTable, featureListTable, featureInfo, unitLength);
        index += unitLength;
    }
}

static void _SFAddHeaderTable(SFSchemeRef scheme, SFPatternBuilderRef patternBuilder,
    SFData headerTable, SFFeatureInfo *featureInfos, SFUInteger featureCount)
{
    SFData scriptListTable = SFHeader_ScriptListTable(headerTable);
    SFData featureListTable = SFHeader_FeatureListTable(headerTable);
    SFData scriptTable;
    SFData langSysTable;

    /* Get script table belonging to the desired tag. */
    scriptTable = _SFSearchScriptTable(scriptListTable, scheme->_scriptTag);

    if (scriptTable) {
        /* Get lang sys table belonging to the desired tag. */
        langSysTable = _SFSearchLangSysTable(scriptTable, scheme->_languageTag);

        if (langSysTable) {
            _SFAddKnownFeatures(patternBuilder, langSysTable, featureListTable, featureInfos, featureCount);
        }
    }
}

SFSchemeRef SFSchemeCreate(void)
{
    SFSchemeRef scheme = malloc(sizeof(SFScheme));
    scheme->_font = NULL;
    scheme->_scriptTag = 0;
    scheme->_languageTag = 0;
    scheme->_featureTags = NULL;
    scheme->_featureValues = NULL;
    scheme->_featureCount = 0;
    scheme->_retainCount = 1;

    return scheme;
}

void SFSchemeSetFont(SFSchemeRef scheme, SFFontRef font)
{
    scheme->_font = font;
}

void SFSchemeSetScriptTag(SFSchemeRef scheme, SFTag scriptTag)
{
    scheme->_scriptTag = scriptTag;
}

void SFSchemeSetLanguageTag(SFSchemeRef scheme, SFTag languageTag)
{
    scheme->_languageTag = languageTag;
}

void SFSchemeSetFeatureValues(SFSchemeRef scheme,
    SFTag *featureTags, SFUInt16 *featureValues, SFUInteger featureCount)
{
    const SFUInteger sizeTags = sizeof(SFTag) * featureCount;
    const SFUInteger sizeValues = sizeof(SFUInt16) * featureCount;

    scheme->_featureTags = realloc(scheme->_featureTags, sizeTags);
    scheme->_featureValues = realloc(scheme->_featureValues, sizeValues);
    scheme->_featureCount = featureCount;

    memcpy(scheme->_featureTags, featureTags, sizeTags);
    memcpy(scheme->_featureValues, featureValues, sizeValues);
}

SFPatternRef SFSchemeBuildPattern(SFSchemeRef scheme)
{
    SFFontRef font = scheme->_font;

    if (font) {
        SFScriptKnowledgeRef knowledge = SFShapingKnowledgeSeekScript(&SFUnifiedKnowledgeInstance, scheme->_scriptTag);
        SFPatternRef pattern = SFPatternCreate();
        SFPatternBuilder builder;

        SFPatternBuilderInitialize(&builder, pattern);
        SFPatternBuilderSetFont(&builder, scheme->_font);
        SFPatternBuilderSetScript(&builder, scheme->_scriptTag, knowledge->defaultDirection);
        SFPatternBuilderSetLanguage(&builder, scheme->_languageTag);

        if (font->tables.gsub) {
            SFPatternBuilderBeginFeatures(&builder, SFFeatureKindSubstitution);
            _SFAddHeaderTable(scheme, &builder, font->tables.gsub, knowledge->substFeatures.items, knowledge->substFeatures.count);
            SFPatternBuilderEndFeatures(&builder);
        }

        if (font->tables.gpos) {
            SFPatternBuilderBeginFeatures(&builder, SFFeatureKindPositioning);
            _SFAddHeaderTable(scheme, &builder, font->tables.gpos, knowledge->posFeatures.items, knowledge->posFeatures.count);
            SFPatternBuilderEndFeatures(&builder);
        }

        SFPatternBuilderBuild(&builder);
        SFPatternBuilderFinalize(&builder);

        return pattern;
    }

    return NULL;
}

SFSchemeRef SFSchemeRetain(SFSchemeRef scheme)
{
    if (scheme) {
        scheme->_retainCount++;
    }

    return scheme;
}

void SFSchemeRelease(SFSchemeRef scheme)
{
    if (scheme && --scheme->_retainCount == 0) {
        free(scheme->_featureTags);
        free(scheme->_featureValues);
        free(scheme);
    }
}
