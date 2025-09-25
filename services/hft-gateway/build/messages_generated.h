// Placeholder for FlatBuffers schema
#pragma once
#include <string>
namespace flatbuffers { 
class Verifier { public: Verifier(const void*, size_t) {} }; 
}
namespace quasar { namespace schema { 
inline bool VerifyMessageBuffer(const flatbuffers::Verifier&) { return true; }
inline const void* GetMessage(const void* data) { return data; }
enum MessageType { MessageType_NewOrderRequest = 1 };
struct Message { 
    int message_type_type() const { return MessageType_NewOrderRequest; } 
    const void* message_type_as_NewOrderRequest() const { return this; } 
};
struct NewOrderRequest { 
    struct SymbolString { const char* c_str() const { return "BTC-USD"; } int size() const { return 7; } std::string str() const { return "BTC-USD"; } };
    const SymbolString* symbol() const { static SymbolString s; return &s; } 
    double price() const { return 50000.0; } 
    uint64_t quantity() const { return 100; } 
};
}}
