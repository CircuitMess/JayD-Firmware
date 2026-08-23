#include "WirelessApiCore.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

namespace WirelessApi {
namespace {

bool reached(uint32_t now, uint32_t deadline){
	return deadline != 0 && static_cast<int32_t>(now - deadline) >= 0;
}

bool copyBounded(char* destination, size_t capacity, const char* source){
	if(!source) return false;
	const size_t length = strlen(source);
	if(length == 0 || length >= capacity) return false;
	memcpy(destination, source, length + 1);
	return true;
}

bool constantTimeEqual(const char* first, const char* second, size_t length){
	uint8_t different = 0;
	for(size_t i = 0; i < length; i++){
		different |= static_cast<uint8_t>(first[i] ^ second[i]);
	}
	return different == 0;
}

void generateToken(char output[TOKEN_CAPACITY], RandomWord random){
	snprintf(
		output,
		TOKEN_CAPACITY,
		"%08lx%08lx%08lx%08lx",
		static_cast<unsigned long>(random()),
		static_cast<unsigned long>(random()),
		static_cast<unsigned long>(random()),
		static_cast<unsigned long>(random())
	);
}

void skipWhitespace(char*& cursor, const char* end){
	while(cursor < end && isspace(static_cast<unsigned char>(*cursor))) cursor++;
}

bool parseQuoted(char*& cursor, const char* end, char* output, size_t capacity){
	if(cursor >= end || *cursor++ != '"') return false;
	size_t written = 0;
	while(cursor < end && *cursor != '"'){
		unsigned char value = static_cast<unsigned char>(*cursor++);
		if(value == '\\'){
			if(cursor >= end) return false;
			const char escaped = *cursor++;
			if(escaped == '"' || escaped == '\\' || escaped == '/') value = escaped;
			else return false;
		}
		if(value < 0x20 || written + 1 >= capacity) return false;
		output[written++] = static_cast<char>(value);
	}
	if(cursor >= end || *cursor++ != '"') return false;
	output[written] = '\0';
	return true;
}

bool parseNumber(char*& cursor, const char* end, uint64_t& value){
	if(cursor >= end || !isdigit(static_cast<unsigned char>(*cursor))) return false;
	value = 0;
	while(cursor < end && isdigit(static_cast<unsigned char>(*cursor))){
		const uint8_t digit = static_cast<uint8_t>(*cursor++ - '0');
		if(value > (UINT64_MAX - digit) / 10) return false;
		value = value * 10 + digit;
	}
	return true;
}

}

bool validClientId(const char* value){
	if(!value) return false;
	const size_t length = strlen(value);
	if(length == 0 || length >= CLIENT_ID_CAPACITY) return false;
	for(size_t i = 0; i < length; i++){
		const unsigned char c = static_cast<unsigned char>(value[i]);
		if(!isalnum(c) && c != '-' && c != '_') return false;
	}
	return true;
}

bool validClientCommandId(const char* value){
	return validClientId(value);
}

bool validToken(const char* value){
	if(!value || strlen(value) != TOKEN_CAPACITY - 1) return false;
	for(size_t i = 0; i < TOKEN_CAPACITY - 1; i++){
		if(!isxdigit(static_cast<unsigned char>(value[i]))) return false;
	}
	return true;
}

bool parseUint64Decimal(const char* text, uint64_t& value){
	if(!text || *text == '\0') return false;
	uint64_t result = 0;
	for(const char* cursor = text; *cursor != '\0'; cursor++){
		if(!isdigit(static_cast<unsigned char>(*cursor))) return false;
		const uint8_t digit = static_cast<uint8_t>(*cursor - '0');
		if(result > (UINT64_MAX - digit) / 10) return false; // would overflow uint64_t
		result = result * 10 + digit;
	}
	value = result;
	return true;
}

void Security::initialize(TokenStore& store){
	StoredTokens loaded = {};
	if(!store.load(loaded) || loaded.version != 1) return;
	for(size_t i = 0; i < MAX_PAIRED_CLIENTS; i++){
		const PairedClient& client = loaded.clients[i];
		if(client.id[0] == '\0' && client.token[0] == '\0') continue;
		if(!validClientId(client.id) || !validToken(client.token)) return;
	}
	tokens = loaded;
}

bool Security::openPairing(uint32_t now, RandomWord random){
	if(!random) return false;
	snprintf(
		pairingCode,
		sizeof(pairingCode),
		"%06lu",
		static_cast<unsigned long>(random() % 1000000)
	);
	pairingDeadline = now + PAIRING_WINDOW_MS;
	if(pairingDeadline == 0) pairingDeadline = 1;
	pairingConsumed = false;
	return true;
}

void Security::expirePairing(uint32_t now){
	if(pairingConsumed || reached(now, pairingDeadline)){
		memset(pairingCode, 0, sizeof(pairingCode));
		pairingDeadline = 0;
	}
}

bool Security::pairingOpen(uint32_t now){
	expirePairing(now);
	return pairingDeadline != 0;
}

uint32_t Security::pairingRemaining(uint32_t now){
	if(!pairingOpen(now)) return 0;
	return pairingDeadline - now;
}

bool Security::copyPairingCode(char output[PAIRING_CODE_CAPACITY], uint32_t now){
	if(!output || !pairingOpen(now)) return false;
	memcpy(output, pairingCode, sizeof(pairingCode));
	return true;
}

PairResult Security::exchangePairing(
	const char* code,
	const char* clientId,
	uint32_t now,
	TokenStore& store,
	RandomWord random,
	char outputToken[TOKEN_CAPACITY]
){
	if(!pairingOpen(now)) return PAIR_CLOSED;
	if(!validClientId(clientId)) return PAIR_INVALID_CLIENT;
	if(!code || strlen(code) != PAIRING_CODE_CAPACITY - 1 ||
	   !constantTimeEqual(code, pairingCode, PAIRING_CODE_CAPACITY - 1)){
		return PAIR_INVALID_CODE;
	}
	if(!random || !outputToken) return PAIR_STORAGE_FAILED;

	size_t slot = MAX_PAIRED_CLIENTS;
	for(size_t i = 0; i < MAX_PAIRED_CLIENTS; i++){
		if(strcmp(tokens.clients[i].id, clientId) == 0){
			slot = i;
			break;
		}
		if(slot == MAX_PAIRED_CLIENTS && tokens.clients[i].id[0] == '\0') slot = i;
	}
	if(slot == MAX_PAIRED_CLIENTS) return PAIR_CLIENT_LIMIT;

	StoredTokens updated = tokens;
	memset(&updated.clients[slot], 0, sizeof(updated.clients[slot]));
	copyBounded(updated.clients[slot].id, sizeof(updated.clients[slot].id), clientId);
	generateToken(updated.clients[slot].token, random);
	if(!store.save(updated)) return PAIR_STORAGE_FAILED;

	tokens = updated;
	memcpy(outputToken, tokens.clients[slot].token, TOKEN_CAPACITY);
	pairingConsumed = true;
	expirePairing(now);
	return PAIR_OK;
}

bool Security::authenticate(const char* authorization, char outputClientId[CLIENT_ID_CAPACITY]){
	static const char prefix[] = "Bearer ";
	if(!authorization || strncmp(authorization, prefix, sizeof(prefix) - 1) != 0){
		stats.authRejects++;
		return false;
	}
	const char* candidate = authorization + sizeof(prefix) - 1;
	if(!validToken(candidate)){
		stats.authRejects++;
		return false;
	}
	for(size_t i = 0; i < MAX_PAIRED_CLIENTS; i++){
		const PairedClient& client = tokens.clients[i];
		if(client.id[0] == '\0' || !constantTimeEqual(candidate, client.token, TOKEN_CAPACITY - 1)) continue;
		if(outputClientId) memcpy(outputClientId, client.id, CLIENT_ID_CAPACITY);
		return true;
	}
	stats.authRejects++;
	return false;
}

bool Security::allowRequest(const char* clientId, bool write, uint32_t now){
	RateEntry* entry = nullptr;
	for(size_t i = 0; i < MAX_PAIRED_CLIENTS; i++){
		if(strcmp(rates[i].clientId, clientId) == 0){
			entry = &rates[i];
			break;
		}
		if(!entry && rates[i].clientId[0] == '\0') entry = &rates[i];
	}
	if(!entry){
		stats.rateRejects++;
		return false;
	}
	if(entry->clientId[0] == '\0') copyBounded(entry->clientId, sizeof(entry->clientId), clientId);
	if(entry->windowStarted == 0 || now - entry->windowStarted >= 1000){
		entry->windowStarted = now == 0 ? 1 : now;
		entry->reads = 0;
		entry->writes = 0;
	}
	uint8_t& count = write ? entry->writes : entry->reads;
	const uint8_t limit = write ? 8 : 20;
	if(count >= limit){
		stats.rateRejects++;
		return false;
	}
	count++;
	return true;
}

bool Security::allowPairAttempt(uint32_t now){
	if(pairWindowStarted == 0 || now - pairWindowStarted >= 60000){
		pairWindowStarted = now == 0 ? 1 : now;
		pairAttempts = 0;
	}
	if(pairAttempts >= 5){
		stats.rateRejects++;
		return false;
	}
	pairAttempts++;
	return true;
}

void Security::expireLease(uint32_t now){
	if(leaseOwner[0] != '\0' && reached(now, leaseDeadline)){
		memset(leaseOwner, 0, sizeof(leaseOwner));
		leaseDeadline = 0;
	}
}

LeaseResult Security::acquireLease(const char* clientId, uint32_t now){
	expireLease(now);
	if(leaseOwner[0] != '\0' && strcmp(leaseOwner, clientId) != 0) return LEASE_CONFLICT;
	copyBounded(leaseOwner, sizeof(leaseOwner), clientId);
	leaseDeadline = now + LEASE_DURATION_MS;
	if(leaseDeadline == 0) leaseDeadline = 1;
	return LEASE_OK;
}

LeaseResult Security::renewLease(const char* clientId, uint32_t now){
	const bool expired = leaseOwner[0] != '\0' && reached(now, leaseDeadline);
	expireLease(now);
	if(leaseOwner[0] == '\0') return expired ? LEASE_EXPIRED : LEASE_NOT_OWNER;
	if(strcmp(leaseOwner, clientId) != 0) return LEASE_NOT_OWNER;
	leaseDeadline = now + LEASE_DURATION_MS;
	if(leaseDeadline == 0) leaseDeadline = 1;
	return LEASE_OK;
}

LeaseResult Security::releaseLease(const char* clientId, uint32_t now){
	const bool expired = leaseOwner[0] != '\0' && reached(now, leaseDeadline);
	expireLease(now);
	if(leaseOwner[0] == '\0') return expired ? LEASE_EXPIRED : LEASE_NOT_OWNER;
	if(strcmp(leaseOwner, clientId) != 0) return LEASE_NOT_OWNER;
	memset(leaseOwner, 0, sizeof(leaseOwner));
	leaseDeadline = 0;
	return LEASE_OK;
}

bool Security::hasWriterLease(const char* clientId, uint32_t now){
	expireLease(now);
	return leaseOwner[0] != '\0' && strcmp(leaseOwner, clientId) == 0;
}

bool Security::copyLeaseOwner(char outputClientId[CLIENT_ID_CAPACITY], uint32_t now){
	expireLease(now);
	if(!outputClientId || leaseOwner[0] == '\0') return false;
	memcpy(outputClientId, leaseOwner, sizeof(leaseOwner));
	return true;
}

uint32_t Security::leaseRemaining(uint32_t now){
	expireLease(now);
	return leaseOwner[0] == '\0' ? 0 : leaseDeadline - now;
}

const Counters& Security::counters() const{
	return stats;
}

bool JsonObject::parse(char* input, size_t length){
	fieldCount = 0;
	if(!input || length == 0 || length > MAX_REQUEST_BODY) return false;
	char* cursor = input;
	const char* end = input + length;
	skipWhitespace(cursor, end);
	if(cursor >= end || *cursor++ != '{') return false;
	skipWhitespace(cursor, end);
	if(cursor < end && *cursor == '}'){
		cursor++;
		skipWhitespace(cursor, end);
		return cursor == end;
	}

	while(cursor < end){
		if(fieldCount >= MAX_JSON_FIELDS) return false;
		JsonField& field = fields[fieldCount];
		memset(&field, 0, sizeof(field));
		if(!parseQuoted(cursor, end, field.name, sizeof(field.name))) return false;
		for(size_t i = 0; i < fieldCount; i++){
			if(strcmp(fields[i].name, field.name) == 0) return false;
		}
		skipWhitespace(cursor, end);
		if(cursor >= end || *cursor++ != ':') return false;
		skipWhitespace(cursor, end);
		if(cursor >= end) return false;
		if(*cursor == '"'){
			field.type = JSON_STRING;
			if(!parseQuoted(cursor, end, field.stringValue, sizeof(field.stringValue))) return false;
		}else if(isdigit(static_cast<unsigned char>(*cursor))){
			field.type = JSON_NUMBER;
			if(!parseNumber(cursor, end, field.numberValue)) return false;
		}else if(end - cursor >= 4 && memcmp(cursor, "true", 4) == 0){
			field.type = JSON_BOOL;
			field.boolValue = true;
			cursor += 4;
		}else if(end - cursor >= 5 && memcmp(cursor, "false", 5) == 0){
			field.type = JSON_BOOL;
			field.boolValue = false;
			cursor += 5;
		}else{
			return false;
		}
		fieldCount++;
		skipWhitespace(cursor, end);
		if(cursor < end && *cursor == ','){
			cursor++;
			skipWhitespace(cursor, end);
			continue;
		}
		if(cursor < end && *cursor == '}'){
			cursor++;
			skipWhitespace(cursor, end);
			return cursor == end;
		}
		return false;
	}
	return false;
}

size_t JsonObject::size() const{
	return fieldCount;
}

bool JsonObject::hasOnly(const char* const* allowed, size_t allowedCount) const{
	for(size_t i = 0; i < fieldCount; i++){
		bool found = false;
		for(size_t j = 0; j < allowedCount; j++){
			if(strcmp(fields[i].name, allowed[j]) == 0){
				found = true;
				break;
			}
		}
		if(!found) return false;
	}
	return true;
}

const JsonField* JsonObject::find(const char* name) const{
	for(size_t i = 0; i < fieldCount; i++){
		if(strcmp(fields[i].name, name) == 0) return &fields[i];
	}
	return nullptr;
}

bool JsonObject::getString(const char* name, const char*& value) const{
	const JsonField* field = find(name);
	if(!field || field->type != JSON_STRING) return false;
	value = field->stringValue;
	return true;
}

bool JsonObject::getNumber(const char* name, uint64_t& value) const{
	const JsonField* field = find(name);
	if(!field || field->type != JSON_NUMBER) return false;
	value = field->numberValue;
	return true;
}

bool JsonObject::getBool(const char* name, bool& value) const{
	const JsonField* field = find(name);
	if(!field || field->type != JSON_BOOL) return false;
	value = field->boolValue;
	return true;
}

}
