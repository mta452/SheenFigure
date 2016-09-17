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

#ifndef _SF_INTERNAL_STANDARD_ENGINE_H
#define _SF_INTERNAL_STANDARD_ENGINE_H

#include <SFConfig.h>

#include "SFArtist.h"
#include "SFShapingEngine.h"
#include "SFShapingKnowledge.h"

typedef struct _SFStandardEngine {
    SFShapingEngine _base;
    SFArtistRef _artist;
} SFStandardEngine, *SFStandardEngineRef;

extern SFShapingKnowledge SFStandardKnowledgeInstance;

SF_INTERNAL void SFStandardEngineInitialize(SFStandardEngineRef standardEngine, SFArtistRef artist);

#endif
