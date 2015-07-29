/* *********************************************************************
* This Source Code Form is copyright of 51Degrees Mobile Experts Limited.
* Copyright 2014 51Degrees Mobile Experts Limited, 5 Charlotte Close,
* Caversham, Reading, Berkshire, United Kingdom RG4 7BY
*
* This Source Code Form is the subject of the following patent
* applications, owned by 51Degrees Mobile Experts Limited of 5 Charlotte
* Close, Caversham, Reading, Berkshire, United Kingdom RG4 7BY:
* European Patent Application No. 13192291.6; and
* United States Patent Application Nos. 14/085,223 and 14/085,301.
*
* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0.
*
* If a copy of the MPL was not distributed with this file, You can obtain
* one at http://mozilla.org/MPL/2.0/.
*
* This Source Code Form is "Incompatible With Secondary Licenses", as
* defined by the Mozilla Public License, v. 2.0.
********************************************************************** */

#include <cstdlib>
#include <stdio.h>
#include <string.h>
#include <node.h>
#include <v8.h>
#include <nan.h>
#include "api.h"
#include "../../../src/pattern/51Degrees.h"

// Default size of the cache if not provided
#define DEFAULT_CACHE_SIZE 5000

// Default size of the work set pool if not provided
#define DEFAULT_POOL_SIZE 10

#ifdef _MSC_VER
#define _INTPTR 0
#endif

using namespace v8;

PatternParser::PatternParser(char *filename, char *requiredProperties, int cacheSize, int poolSize) {
	dataSet = (fiftyoneDegreesDataSet*)malloc(sizeof(fiftyoneDegreesDataSet));
	initResult = fiftyoneDegreesInitWithPropertyString(filename, dataSet, requiredProperties);
	cache = fiftyoneDegreesResultsetCacheCreate(dataSet, cacheSize);
	pool = fiftyoneDegreesWorksetPoolCreate(dataSet, cache, poolSize);
}

PatternParser::PatternParser(char *filename, char *requiredProperties) :
PatternParser(filename, requiredProperties, DEFAULT_CACHE_SIZE, DEFAULT_POOL_SIZE) {}

PatternParser::~PatternParser() {
	if (pool != NULL) {
		fiftyoneDegreesWorksetPoolFree(pool);
		pool = NULL;
	}
	if (cache != NULL) {
		fiftyoneDegreesResultsetCacheFree(cache);
		cache = NULL;
	}
	if (dataSet != NULL) {
		fiftyoneDegreesDataSetFree(dataSet);
		dataSet = NULL;
	}
}

fiftyoneDegreesWorkset* PatternParser::GetWorkset() {
	return fiftyoneDegreesWorksetPoolGet(pool);
}

void PatternParser::ReleaseWorkset(fiftyoneDegreesWorkset *ws) {
	fiftyoneDegreesWorksetPoolRelease(pool, ws);
}

void PatternParser::Init(Handle<Object> target) {
	NanScope();
	Local<FunctionTemplate> t = NanNew<FunctionTemplate>(New);
	// TODO(Yorkie): will remove
	t->InstanceTemplate()->SetInternalFieldCount(1);
	NODE_SET_PROTOTYPE_METHOD(t, "parse", Parse);
	target->Set(NanNew<v8::String>("PatternParser"), t->GetFunction());
}

NAN_METHOD(PatternParser::New) {
	NanScope();
	char *filename;
	char *requiredProperties;

	// convert v8 objects to c/c++ types
	v8::String::Utf8Value v8_filename(args[0]->ToString());
	v8::String::Utf8Value v8_properties(args[1]->ToString());
	filename = *v8_filename;
	requiredProperties = *v8_properties;

	// create new instance of C++ class PatternParser
	PatternParser *parser = new PatternParser(filename, requiredProperties);
	parser->Wrap(args.This());

	// Check the response 
	switch (parser->initResult) {
		case DATA_SET_INIT_STATUS_NOT_SET:
			return NanThrowError("Device data file could not be initialised");
		case DATA_SET_INIT_STATUS_INSUFFICIENT_MEMORY:
			return NanThrowError("Insufficient memory");
		case DATA_SET_INIT_STATUS_CORRUPT_DATA:
			return NanThrowError("Device data file is corrupted");
		case DATA_SET_INIT_STATUS_INCORRECT_VERSION:
			return NanThrowError("Device data file is not correct");
		case DATA_SET_INIT_STATUS_FILE_NOT_FOUND:
			return NanThrowError("Device data file not found");
		default:
			NanReturnValue(args.This());
	}
}

NAN_METHOD(PatternParser::Parse) {
	NanScope();

	// convert v8 objects to c/c++ types
	PatternParser *parser = ObjectWrap::Unwrap<PatternParser>(args.This());
	Local<Object> result = NanNew<Object>();
	v8::String::Utf8Value v8_input(args[0]->ToString());

	fiftyoneDegreesWorkset *ws = parser->GetWorkset();
	
	// here we should initialize the ws->input by hand for avoiding
	// memory incropted.
	size_t userAgentLength = strlen(*v8_input);
	memcpy(
		ws->input, 
		*v8_input,
		userAgentLength > parser->dataSet->header.maxUserAgentLength ? 
		parser->dataSet->header.maxUserAgentLength : userAgentLength);
	fiftyoneDegreesMatch(ws, ws->input);

	if (ws->profileCount > 0) {

		// here we fetch ID
		int32_t propertyIndex, valueIndex, profileIndex;
		int idSize = ws->profileCount * 5 + (ws->profileCount - 1) + 1;
		char *ids = (char*)malloc(idSize);
		char *pos = ids;
		for (profileIndex = 0; profileIndex < ws->profileCount; profileIndex++) {
			if (profileIndex < ws->profileCount - 1)
				pos += snprintf(pos, idSize, "%d-", (*(ws->profiles + profileIndex))->profileId);
			else
				pos += snprintf(pos, idSize, "%d", (*(ws->profiles + profileIndex))->profileId);
		}
		result->Set(NanNew<v8::String>("Id"), NanNew<v8::String>(ids));
		free(ids);

		// build JSON
		for (propertyIndex = 0;
			propertyIndex < ws->dataSet->requiredPropertyCount;
			propertyIndex++) {

			if (fiftyoneDegreesSetValues(ws, propertyIndex) <= 0)
				break;

			const char *key = fiftyoneDegreesGetPropertyName(ws->dataSet,
				*(ws->dataSet->requiredProperties + propertyIndex));

			if (ws->valuesCount == 1) {
				const char *val = fiftyoneDegreesGetValueName(ws->dataSet, *(ws->values));
				// convert string to boolean
				if (strcmp(val, "True") == 0)
					result->Set(NanNew<v8::String>(key), NanTrue());
				else if (strcmp(val, "False") == 0)
					result->Set(NanNew<v8::String>(key), NanFalse());
				else
					result->Set(NanNew<v8::String>(key), NanNew<v8::String>(val));
			}
			else {
				Local<Array> vals = NanNew<Array>(ws->valuesCount - 1);
				for (valueIndex = 0; valueIndex < ws->valuesCount; valueIndex++) {
					const char *val = fiftyoneDegreesGetValueName(ws->dataSet, *(ws->values + valueIndex));
					vals->Set(valueIndex, NanNew<v8::String>(val));
				}
				result->Set(NanNew<v8::String>(key), vals);
			}
		}

		Local<Object> meta = NanNew<Object>();
		meta->Set(NanNew<v8::String>("difference"), NanNew<v8::Integer>(ws->difference));
		meta->Set(NanNew<v8::String>("method"), NanNew<v8::Integer>(ws->method));
		meta->Set(NanNew<v8::String>("rank"), NanNew<v8::Integer>(fiftyoneDegreesGetSignatureRank(ws)));
		meta->Set(NanNew<v8::String>("rootNodesEvaluated"), NanNew<v8::Integer>(ws->rootNodesEvaluated));
		meta->Set(NanNew<v8::String>("nodesEvaluated"), NanNew<v8::Integer>(ws->nodesEvaluated));
		meta->Set(NanNew<v8::String>("stringsRead"), NanNew<v8::Integer>(ws->stringsRead));
		meta->Set(NanNew<v8::String>("signaturesRead"), NanNew<v8::Integer>(ws->signaturesRead));
		meta->Set(NanNew<v8::String>("signaturesCompared"), NanNew<v8::Integer>(ws->signaturesCompared));
		meta->Set(NanNew<v8::String>("closestSignatures"), NanNew<v8::Integer>(ws->closestSignatures));
		result->Set(NanNew<v8::String>("__meta__"), meta);
	}
	else {
		NanFalse();
	}

	// Release the work set back into the pool.
	parser->ReleaseWorkset(ws);

	NanReturnValue(result);
}

NODE_MODULE(pattern, PatternParser::Init)