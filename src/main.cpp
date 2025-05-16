#include <TFT_eSPI.h>
#include <SPI.h>
#include <Arduino.h>
#include <DHT.h>
#include <Arduino.h>
#include <WiFi.h>
#include "IoTxChain-lib.h"

// Track last transaction signature and initialization state
String lastTxSignature;
bool initDone = false;

#define DHTPIN 21
#define DHTTYPE DHT11

// // WiFi credentials
const char* ssid     = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";

// Your Solana wallet (Base58 format)
const String PRIVATE_KEY_BASE58 = "PRIVATE_KEY_BASE58";  // 64-byte base58


const String PUBLIC_KEY_BASE58 = "PUBLIC_KEY_BASE58"; //My wallet address


// Solana RPC URL (Devnet)
const String solanaRpcUrl = "https://api.devnet.solana.com"; // or mainnet/testnet


const String ProgramIdBase58  = "YOUR_PROGRAM_ID"; //

// Initialize Solana Library
IoTxChain solana(solanaRpcUrl);

DHT dht(DHTPIN, DHTTYPE);

// Added generalized Anchor Instruction with PDA function
void example_sendAnchorInstructionWithPDA(
    const std::string& functionName,
    const std::vector<std::vector<uint8_t>>& customSeeds,
    const std::vector<uint8_t>& payload
) {
    Serial.println("\n=== 🔹 Anchor Instruction with PDA (Generic) ===");

    uint8_t privateKey[128];
    size_t privLen = sizeof(privateKey);
    if (!base58Decode(PRIVATE_KEY_BASE58, privateKey, privLen) || privLen < 64) {
        Serial.println("❌ Private key decode failed");
        return;
    }

    Pubkey authority = Pubkey::fromBase58(PUBLIC_KEY_BASE58);
    Keypair signer = Keypair::fromPrivateKey(privateKey);

    std::vector<uint8_t> programId = base58ToPubkey(ProgramIdBase58);

    Pubkey pda = Pubkey::fromBase58("YOUR_PDA"); //PDA

    std::vector<uint8_t> discriminator = solana.calculateDiscriminator(functionName);
    std::vector<uint8_t> data = discriminator;
    data.insert(data.end(), payload.begin(), payload.end());

    Instruction ix(
        Pubkey{programId},
        functionName == "initialize" ? std::vector<AccountMeta>{
            AccountMeta::writable(pda, false),
            AccountMeta::writable(authority, true),
            AccountMeta{Pubkey::fromBase58("11111111111111111111111111111111"), false, false}
        } : std::vector<AccountMeta>{
            AccountMeta::writable(pda, false),
            AccountMeta::signer(authority)
        },
        data
    );

    Transaction tx;
    tx.fee_payer = authority;
    tx.recent_blockhash = solana.getLatestBlockhash();
    if (tx.recent_blockhash.isEmpty()) {
        Serial.println("❌ Failed to get blockhash!");
        return;
    }
    tx.add(ix);
    tx.sign({signer});
    String txBase64 = tx.serializeBase64();

    String txSig;
    if (solana.sendRawTransaction(txBase64, txSig)) {
        lastTxSignature = txSig;
        Serial.println("✅ Anchor tx sent! Signature: " + txSig);
    } else {
        Serial.println("❌ Anchor tx failed.");
    }
}

void example_confirmTransaction(String txSignature) {
    Serial.println("\n=== 🔹 confirmTransaction() ===");

   //  String txSignature = "BASE58_SIGNATURE_STRING_HERE";

    if (solana.confirmTransaction(txSignature, 5000)) {
        Serial.println("✅ Transaction Confirmed.");
    } else {
        Serial.println("❌ Transaction NOT Confirmed.");
    }
}

void example_callInitialize() {
    std::vector<std::vector<uint8_t>> seeds = {
        {'t','e','m','p','_','d','a','t','a'},
        base58ToPubkey(PUBLIC_KEY_BASE58)
    };

    std::vector<uint8_t> payload;  // no payload for initialize
    example_sendAnchorInstructionWithPDA("initialize", seeds, payload);
    // Confirm initialize transaction
    example_confirmTransaction(lastTxSignature);
    // Mark initialization done
    initDone = true;
}

void example_callUpdateTemperature(int64_t temperature, int64_t humidity) {
    std::vector<std::vector<uint8_t>> seeds = {
        {'t','e','m','p','_','d','a','t','a'},
        base58ToPubkey(PUBLIC_KEY_BASE58)
    };

    std::vector<uint8_t> payload;
    std::vector<uint8_t> tempEncoded = encodeU64LE((uint64_t)temperature);
    std::vector<uint8_t> humidityEncoded = encodeU64LE((uint64_t)humidity);

    payload.insert(payload.end(), tempEncoded.begin(), tempEncoded.end());
    payload.insert(payload.end(), humidityEncoded.begin(), humidityEncoded.end());
    
    example_sendAnchorInstructionWithPDA("update_temperature", seeds, payload);
}

void setup() {

   //mywallet
   //FgtXBob7pvpxeha8EjEn1Dt8SLhurj845BHRUKZzQddj
   
    Serial.begin(115200);

    // Connect to Wi-Fi
    WiFi.begin(ssid, password);
    Serial.print("Connecting to WiFi");
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println(" - Connected!");
    
    dht.begin();
    example_callInitialize();
}

void loop() {

  delay(5000);

  int64_t humidity = dht.readHumidity();
  int64_t temperature = dht.readTemperature();

  if (isnan(humidity) || isnan(temperature)) {
    Serial.println("❌ Failed to read data from DHT11 sensor!");
    return;
  }

  Serial.print("🌡️ temperature: ");
  Serial.print(temperature, 0);
  Serial.print(" °C  |  ");
  Serial.print("💧 humidity: ");
  Serial.print(humidity, 0);
  Serial.println(" %");
  // Send update_temperature transaction if initialization is completed
  if (initDone) {
      example_callUpdateTemperature(temperature, humidity);
  }
}