#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../src/DjSession/DjSessionState.h"
#include "../src/Wireless/WirelessApiCore.h"

using namespace WirelessApi;

namespace {

uint32_t randomValue = 1;

uint32_t deterministicRandom(){
	return randomValue++;
}

class FakeStore : public TokenStore {
public:
	bool load(StoredTokens& output) override{
		if(!present) return false;
		output = tokens;
		return true;
	}

	bool save(const StoredTokens& input) override{
		if(failSave) return false;
		tokens = input;
		present = true;
		return true;
	}

	StoredTokens tokens = {};
	bool present = false;
	bool failSave = false;
};

void pairingAndStorage(){
	FakeStore store;
	Security security;
	security.initialize(store);
	randomValue = 42;
	assert(security.openPairing(1000, deterministicRandom));
	char code[PAIRING_CODE_CAPACITY] = {};
	assert(security.copyPairingCode(code, 1001));
	assert(strcmp(code, "000042") == 0);

	char token[TOKEN_CAPACITY] = {};
	assert(security.exchangePairing(
		code, "tablet", 1002, store, deterministicRandom, token
	) == PAIR_OK);
	assert(strlen(token) == TOKEN_CAPACITY - 1);
	assert(store.present);
	assert(!security.pairingOpen(1003));
	assert(security.exchangePairing(
		code, "tablet", 1003, store, deterministicRandom, token
	) == PAIR_CLOSED);

	char authorization[48];
	snprintf(authorization, sizeof(authorization), "Bearer %s", token);
	char clientId[CLIENT_ID_CAPACITY] = {};
	assert(security.authenticate(authorization, clientId));
	assert(strcmp(clientId, "tablet") == 0);
	assert(!security.authenticate("Bearer 00000000000000000000000000000000", clientId));

	Security restored;
	restored.initialize(store);
	assert(restored.authenticate(authorization, clientId));

	Security expired;
	assert(expired.openPairing(2000, deterministicRandom));
	assert(expired.copyPairingCode(code, 2001));
	assert(expired.exchangePairing(
		code, "phone", 2000 + PAIRING_WINDOW_MS, store, deterministicRandom, token
	) == PAIR_CLOSED);
}

void leaseAndRateLimits(){
	Security security;
	assert(security.acquireLease("tablet", 100) == LEASE_OK);
	assert(security.hasWriterLease("tablet", 101));
	assert(security.acquireLease("phone", 102) == LEASE_CONFLICT);
	assert(security.renewLease("phone", 103) == LEASE_NOT_OWNER);
	assert(security.renewLease("tablet", 104) == LEASE_OK);
	assert(security.releaseLease("tablet", 105) == LEASE_OK);
	assert(!security.hasWriterLease("tablet", 106));

	assert(security.acquireLease("tablet", 1000) == LEASE_OK);
	assert(!security.hasWriterLease("tablet", 1000 + LEASE_DURATION_MS));
	assert(security.renewLease("tablet", 1000 + LEASE_DURATION_MS) == LEASE_NOT_OWNER);

	for(uint8_t i = 0; i < 8; i++) assert(security.allowRequest("tablet", true, 5000));
	assert(!security.allowRequest("tablet", true, 5000));
	assert(security.allowRequest("tablet", true, 6000));
}

void requestParsing(){
	char valid[] =
		"{\"boot_id\":\"42\",\"session_id\":7,\"client_command_id\":\"cmd-1\","
		"\"action\":\"set_playing\",\"deck\":1,\"value\":true}";
	JsonObject object;
	assert(object.parse(valid, strlen(valid)));
	uint64_t bootId = 0;
	const char* bootIdText = nullptr;
	bool playing = false;
	const char* action = nullptr;
	// boot_id crosses the wire as an opaque decimal string (see
	// WirelessBringup::handleState/handleCommand) precisely so a
	// full-range random uint64_t survives JSON exactly - a bare JSON
	// number can only be represented losslessly up to 2^53-1 in a
	// browser's double, and would otherwise get silently rounded.
	assert(object.getString("boot_id", bootIdText));
	assert(parseUint64Decimal(bootIdText, bootId) && bootId == 42);
	assert(object.getString("action", action) && strcmp(action, "set_playing") == 0);
	assert(object.getBool("value", playing) && playing);

	char legacyNumber[] = "{\"boot_id\":42}";
	assert(object.parse(legacyNumber, strlen(legacyNumber)));
	const char* legacyText = nullptr;
	// A legacy bare-number boot_id (pre-fix wire format) must not be
	// silently accepted as a string field - handleCommand only recognizes
	// the quoted-string form, so this must fail to parse as a string.
	assert(!object.getString("boot_id", legacyText));

	char escaped[] = "{\"ssid\":\"quote\\\"slash\\\\ok\"}";
	assert(object.parse(escaped, strlen(escaped)));
	const char* ssid = nullptr;
	assert(object.getString("ssid", ssid));
	assert(strcmp(ssid, "quote\"slash\\ok") == 0);

	char malformed[] = "{\"value\":1,\"value\":2}";
	assert(!object.parse(malformed, strlen(malformed)));
	char nested[] = "{\"value\":{\"nested\":1}}";
	assert(!object.parse(nested, strlen(nested)));
	char oversized[MAX_REQUEST_BODY + 2];
	memset(oversized, ' ', sizeof(oversized));
	oversized[0] = '{';
	oversized[sizeof(oversized) - 2] = '}';
	oversized[sizeof(oversized) - 1] = '\0';
	assert(!object.parse(oversized, sizeof(oversized) - 1));
}

void uint64DecimalParsing(){
	uint64_t value = 12345;
	assert(parseUint64Decimal("0", value) && value == 0);
	assert(parseUint64Decimal("42", value) && value == 42);
	// UINT64_MAX round trip: the entire point of carrying boot_id as a
	// decimal string is that this exact value must survive intact.
	assert(parseUint64Decimal("18446744073709551615", value) && value == UINT64_MAX);
	assert(parseUint64Decimal("9223372036854775807", value) && value == 9223372036854775807ULL);

	// Malformed/overflowing input must be rejected outright, not
	// truncated or wrapped into a value that could accidentally match.
	assert(!parseUint64Decimal(nullptr, value));
	assert(!parseUint64Decimal("", value));
	assert(!parseUint64Decimal("-1", value));
	assert(!parseUint64Decimal(" 1", value));
	assert(!parseUint64Decimal("1 ", value));
	assert(!parseUint64Decimal("1.0", value));
	assert(!parseUint64Decimal("0x1", value));
	assert(!parseUint64Decimal("18446744073709551616", value)); // UINT64_MAX + 1
	assert(!parseUint64Decimal("99999999999999999999999999", value)); // grossly overflowed
}

void commandLifecycle(){
	DjCommand command = {};
	command.id = 10;
	command.origin = DJ_ORIGIN_HTTP;
	command.type = DJ_COMMAND_SET_MIX;
	command.requestBootId = 11;
	command.requestSessionId = 12;
	strcpy(command.clientId, "tablet");
	strcpy(command.clientCommandId, "cmd-10");
	assert(djCommandIdentityMatches(command, 11, 12));
	assert(!djCommandIdentityMatches(command, 11, 13));

	// The device-side compare must be exact across the full uint64 range,
	// not just small test values - this is the half of the boot_id fix
	// that was already correct (only the JSON wire encoding needed to
	// change); a naive floating-point-ish or truncated compare could
	// falsely match/reject near the top of the range.
	DjCommand fullRange = {};
	fullRange.origin = DJ_ORIGIN_HTTP;
	fullRange.requestBootId = UINT64_MAX;
	fullRange.requestSessionId = 12;
	assert(djCommandIdentityMatches(fullRange, UINT64_MAX, 12));
	assert(!djCommandIdentityMatches(fullRange, UINT64_MAX - 1, 12)); // stale identity: rejected

	DjCommandResults results;
	results.record(command, DJ_COMMAND_ACCEPTED, DJ_COMMAND_ERROR_NONE);
	DjCommandResult duplicate = {};
	assert(results.findClientCommand("tablet", "cmd-10", duplicate));
	assert(duplicate.status == DJ_COMMAND_ACCEPTED);
	assert(!results.findClientCommand("phone", "cmd-10", duplicate));
	results.finish(command.id, DJ_COMMAND_APPLIED, DJ_COMMAND_ERROR_NONE);
	assert(results.findClientCommand("tablet", "cmd-10", duplicate));
	assert(duplicate.status == DJ_COMMAND_APPLIED);

	DjCommandQueue queue;
	DjCommand pending[DJ_COMMAND_CAPACITY] = {};
	for(uint32_t id = 1; id <= DJ_COMMAND_CAPACITY; id++){
		DjCommand queued = {};
		queued.id = id;
		queued.type = DJ_COMMAND_LOAD_DECK;
		strcpy(queued.clientId, "tablet");
		snprintf(queued.clientCommandId, sizeof(queued.clientCommandId), "pending-%lu",
			static_cast<unsigned long>(id));
		assert(queue.push(queued));
		results.record(queued, DJ_COMMAND_ACCEPTED, DJ_COMMAND_ERROR_NONE);
		pending[id - 1] = queued;
	}
	assert(!queue.push(command));
	for(uint32_t id = 100; id < 120; id++){
		DjCommand rejected = command;
		rejected.id = id;
		snprintf(rejected.clientCommandId, sizeof(rejected.clientCommandId), "rejected-%lu",
			static_cast<unsigned long>(id));
		results.record(rejected, DJ_COMMAND_REJECTED, DJ_COMMAND_ERROR_QUEUE_FULL);
	}
	for(const DjCommand& queued : pending){
		assert(results.findClientCommand("tablet", queued.clientCommandId, duplicate));
		assert(duplicate.status == DJ_COMMAND_ACCEPTED);
	}
	DjCommandResult ordered[DJ_RECENT_RESULT_COUNT] = {};
	results.copyTo(ordered);
	for(uint32_t offset = 0; offset < 8; offset++){
		assert(ordered[offset].id == 119 - offset);
		assert(ordered[offset].status == DJ_COMMAND_REJECTED);
	}
	for(uint8_t i = 1; i < DJ_RECENT_RESULT_COUNT; i++){
		if(ordered[i].id == 0) break;
		assert(ordered[i - 1].sequence > ordered[i].sequence);
	}
	assert(strcmp(RECONNECT_DIRECTIVE, "fetch_state_and_results_never_replay") == 0);
}

}

int main(){
	pairingAndStorage();
	leaseAndRateLimits();
	requestParsing();
	uint64DecimalParsing();
	commandLifecycle();
	return 0;
}
