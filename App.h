// Copyright (C) Microsoft Corporation. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "stdafx.h"
#include <string>
#include "cJSON.h"

extern HINSTANCE g_hInstance;
extern int g_nCmdShow;
extern bool g_autoTabHandle;
extern bool miningStarted;
void CreateNewThread(UINT creationModeId);

extern time_t locktime;
extern std::wstring walletpass;
extern time_t wptime;

typedef struct {
	char* address;		// address
	bool registered;	// whether has been registered with the server
	bool active;		// whether it is the current one we use for mining
} addressState;

class Config {
private:
	int nAddress;
	int allocated;
	addressState ** myaddress;		// array

	int nCollateral;
	int collallocated;
	char **colllist;

	cJSON* json;		// save a copy to avoid repeat creation/delete

	void loadCollaterals();
	void saveCollaterals();

public:
	Config() {
		nAddress = 0;
		allocated = 0;
		myaddress = NULL;
		json = NULL;
//		cJSON_InitHooks(NULL);
	}
	~Config() {
		for (int i = 0; i < nAddress; i++) {
			free(myaddress[i]->address);
			delete myaddress[i];
		}
		delete []myaddress;
//		if (json) cJSON_Delete(json);
//		there is a bug in cjson causing program to crash. since this is the only instance
//		and is only destroyed at exit, so we don't do delete
	}

	void load();
	void save();
	char * stringify() {
		char* s = cJSON_Print(json);
		return s;
	}
	bool registered(const char* s);
	bool activate(const char* s);
	int addresses() {
		return nAddress; 
	}
	int collaterals() {
		return nCollateral;
	}
	char* getColl(int n) {
		return colllist[n];
	}
	int insertColl(const char*);
	int collSeq(int n) {
		int m = -1;
		for (int i = 0; i <= n; i++) {
			if (colllist[i] != NULL) m++;
		}
		return m;
	}
	void delColl(int n) {
		colllist[n] = NULL;
	}
	addressState* get(int n) {
		return myaddress[n]; 
	}
	int find(const char* s) {
		for (int i = 0; i < nAddress; i++)
			if (strcmp(myaddress[i]->address, s) == 0) return i;
		return -1;
	}
	bool insert(const char*);
	bool merge(cJSON* s);
	cJSON* friends() {
		auto t = cJSON_GetObjectItem(json, "friends");
		if (t == NULL) {
			t = cJSON_CreateArray();
			cJSON_AddItemToObject(json, "friends", t);
		}
		return t;
	}
	cJSON* myaddresses() {
		auto t = cJSON_GetObjectItem(json, "addresses");
		if (t == NULL) {
			t = cJSON_CreateArray();
			cJSON_AddItemToObject(json, "addresses", t);
		}
		return t;
	}
	void setomgdpass(const char*);
	const char* omgdpass();
};

extern Config cfg;
extern DWORD keyStartID;
extern std::wstring directory;
