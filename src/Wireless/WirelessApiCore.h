#ifndef JAYD_WIRELESS_API_CORE_H
#define JAYD_WIRELESS_API_CORE_H

#include <stddef.h>
#include <stdint.h>

namespace WirelessApi {

static constexpr size_t CLIENT_ID_CAPACITY = 33;
static constexpr size_t TOKEN_CAPACITY = 33;
static constexpr size_t PAIRING_CODE_CAPACITY = 7;
static constexpr size_t JSON_STRING_CAPACITY = 65;
static constexpr size_t MAX_PAIRED_CLIENTS = 4;
static constexpr size_t MAX_JSON_FIELDS = 10;
static constexpr size_t MAX_REQUEST_BODY = 512;
static constexpr uint32_t PAIRING_WINDOW_MS = 60000;
static constexpr uint32_t LEASE_DURATION_MS = 15000;
static constexpr const char* RECONNECT_DIRECTIVE = "fetch_state_and_results_never_replay";

using RandomWord = uint32_t (*)();

struct PairedClient {
	char id[CLIENT_ID_CAPACITY] = {};
	char token[TOKEN_CAPACITY] = {};
};

struct StoredTokens {
	uint32_t version = 1;
	PairedClient clients[MAX_PAIRED_CLIENTS] = {};
};

class TokenStore {
public:
	virtual ~TokenStore() = default;
	virtual bool load(StoredTokens& tokens) = 0;
	virtual bool save(const StoredTokens& tokens) = 0;
};

enum PairResult : uint8_t {
	PAIR_OK,
	PAIR_CLOSED,
	PAIR_INVALID_CODE,
	PAIR_INVALID_CLIENT,
	PAIR_CLIENT_LIMIT,
	PAIR_STORAGE_FAILED
};

enum LeaseResult : uint8_t {
	LEASE_OK,
	LEASE_CONFLICT,
	LEASE_NOT_OWNER,
	LEASE_EXPIRED
};

struct Counters {
	uint32_t authRejects = 0;
	uint32_t rateRejects = 0;
};

class Security {
public:
	void initialize(TokenStore& store);
	bool openPairing(uint32_t now, RandomWord random);
	bool pairingOpen(uint32_t now);
	uint32_t pairingRemaining(uint32_t now);
	bool copyPairingCode(char output[PAIRING_CODE_CAPACITY], uint32_t now);
	PairResult exchangePairing(
		const char* code,
		const char* clientId,
		uint32_t now,
		TokenStore& store,
		RandomWord random,
		char outputToken[TOKEN_CAPACITY]
	);
	bool authenticate(const char* authorization, char outputClientId[CLIENT_ID_CAPACITY]);
	bool allowRequest(const char* clientId, bool write, uint32_t now);
	bool allowPairAttempt(uint32_t now);
	LeaseResult acquireLease(const char* clientId, uint32_t now);
	LeaseResult renewLease(const char* clientId, uint32_t now);
	LeaseResult releaseLease(const char* clientId, uint32_t now);
	bool hasWriterLease(const char* clientId, uint32_t now);
	bool copyLeaseOwner(char outputClientId[CLIENT_ID_CAPACITY], uint32_t now);
	uint32_t leaseRemaining(uint32_t now);
	const Counters& counters() const;

private:
	struct RateEntry {
		char clientId[CLIENT_ID_CAPACITY] = {};
		uint32_t windowStarted = 0;
		uint8_t reads = 0;
		uint8_t writes = 0;
	};

	StoredTokens tokens = {};
	RateEntry rates[MAX_PAIRED_CLIENTS] = {};
	char pairingCode[PAIRING_CODE_CAPACITY] = {};
	uint32_t pairingDeadline = 0;
	bool pairingConsumed = false;
	char leaseOwner[CLIENT_ID_CAPACITY] = {};
	uint32_t leaseDeadline = 0;
	uint32_t pairWindowStarted = 0;
	uint8_t pairAttempts = 0;
	Counters stats = {};

	void expirePairing(uint32_t now);
	void expireLease(uint32_t now);
};

enum JsonValueType : uint8_t {
	JSON_STRING,
	JSON_NUMBER,
	JSON_BOOL
};

struct JsonField {
	char name[CLIENT_ID_CAPACITY] = {};
	JsonValueType type = JSON_STRING;
	char stringValue[JSON_STRING_CAPACITY] = {};
	uint64_t numberValue = 0;
	bool boolValue = false;
};

class JsonObject {
public:
	bool parse(char* input, size_t length);
	size_t size() const;
	bool hasOnly(const char* const* allowed, size_t allowedCount) const;
	bool getString(const char* name, const char*& value) const;
	bool getNumber(const char* name, uint64_t& value) const;
	bool getBool(const char* name, bool& value) const;

private:
	JsonField fields[MAX_JSON_FIELDS] = {};
	size_t fieldCount = 0;

	const JsonField* find(const char* name) const;
};

bool validClientId(const char* value);
bool validClientCommandId(const char* value);
bool validToken(const char* value);

// Strict decimal-string -> uint64_t parse for opaque wire identifiers
// (boot_id) that must survive JSON exactly: JS Number can only represent
// integers exactly up to 2^53-1, so a full-range random uint64 sent as a
// bare JSON number silently loses precision in the browser. Carrying it as
// a quoted decimal string instead means the value only ever needs an exact
// text round trip, never numeric coercion, on either side.
//
// Rejects empty input, any non-digit character, and overflow (a value that
// would not fit in 64 bits) rather than silently truncating/wrapping.
bool parseUint64Decimal(const char* text, uint64_t& value);

}

#endif
