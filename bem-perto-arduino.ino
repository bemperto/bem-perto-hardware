#include <SPI.h>
#include <MFRC522.h>

#define SS_PIN 10
#define RST_PIN 9
#define LED_VERMELHO 6
#define LED_VERDE 4
#define LED_AZUL 5

#define TEMPO_BLINK 250
#define VEZES_BLINK 3
#define TEMPO_ACESSO_LIBERADO 5000
#define TEMPO_ACESSO_NEGADO 3000

struct Usuario {
  String uid;
  String nome;
  byte ledR;
  byte ledG;
  byte ledB;
};

// Authorized users array
const int NUM_USUARIOS = 5;
Usuario usuarios[NUM_USUARIOS] = {
  {"31 CF 75 A4", "Chaveiro", 0, 255, 0},      // Verde
  {"D2 EA F6 00", "Vinicius", 0, 255, 0},      // Verde
  {"22 CD F0 00", "Raposo", 0, 255, 255},      // Verde + Azul
  {"1E D1 A5 39", "Patrick", 255, 0, 255},      // Vermelho + Azul
  {"02 2C F4 00", "Pedro V.C. Bedor", 0,255,0}
};

MFRC522 mfrc522(SS_PIN, RST_PIN);

void configurarLEDs();
void piscarAzul(int vezes);
void acenderLED(byte r, byte g, byte b, int tempo);
void desligarLEDs();
String lerUID();
int buscarUsuario(String uid);
void concederAcesso(Usuario usuario);
void negarAcesso();

void setup()
{
  Serial.begin(9600);
  SPI.begin();
  mfrc522.PCD_Init();

  configurarLEDs();

  Serial.println("Aproxime o seu cartao do leitor...");
  Serial.println();
}

void loop()
{
  // Look for new cards
  if (!mfrc522.PICC_IsNewCardPresent())
  {
    return;
  }

  // Select one of the cards
  if (!mfrc522.PICC_ReadCardSerial())
  {
    return;
  }

  String uid = lerUID();
  Serial.print("UID da tag :");
  Serial.println(uid);
  Serial.print("Mensagem : ");

  piscarAzul(VEZES_BLINK);

  int usuarioIndex = buscarUsuario(uid);

  if (usuarioIndex >= 0)
  {
    concederAcesso(usuarios[usuarioIndex]);
  }
  else
  {
    negarAcesso();
  }
}

void configurarLEDs()
{
  pinMode(LED_VERMELHO, OUTPUT);
  pinMode(LED_VERDE, OUTPUT);
  pinMode(LED_AZUL, OUTPUT);
  desligarLEDs();
}

void piscarAzul(int vezes)
{
  for (int i = 0; i < vezes; i++)
  {
    digitalWrite(LED_AZUL, HIGH);
    delay(TEMPO_BLINK);
    digitalWrite(LED_AZUL, LOW);
    delay(TEMPO_BLINK);
  }
}

void acenderLED(byte r, byte g, byte b, int tempo)
{
  digitalWrite(LED_VERMELHO, r > 0 ? HIGH : LOW);
  digitalWrite(LED_VERDE, g > 0 ? HIGH : LOW);
  digitalWrite(LED_AZUL, b > 0 ? HIGH : LOW);
  delay(tempo);
  desligarLEDs();
}

// Turn off all LEDs
void desligarLEDs()
{
  digitalWrite(LED_VERMELHO, LOW);
  digitalWrite(LED_VERDE, LOW);
  digitalWrite(LED_AZUL, LOW);
}

// Read UID from card
String lerUID()
{
  String conteudo = "";
  for (byte i = 0; i < mfrc522.uid.size; i++)
  {
    conteudo.concat(String(mfrc522.uid.uidByte[i] < 0x10 ? " 0" : " "));
    conteudo.concat(String(mfrc522.uid.uidByte[i], HEX));
  }
  conteudo.toUpperCase();
  return conteudo.substring(1); // Remove leading space
}

// Search for user by UID
int buscarUsuario(String uid)
{
  for (int i = 0; i < NUM_USUARIOS; i++)
  {
    if (uid == usuarios[i].uid)
    {
      return i;
    }
  }
  return -1; // User not found
}

// Grant access to authorized user
void concederAcesso(Usuario usuario)
{
  Serial.print("Ola, ");
  Serial.print(usuario.nome);
  Serial.println(" !");
  Serial.println("Acesso liberado!");
  Serial.println();

  acenderLED(usuario.ledR, usuario.ledG, usuario.ledB, TEMPO_ACESSO_LIBERADO);
}

// Deny access to unauthorized user
void negarAcesso()
{
  Serial.println("Acesso Negado! FILHA DA PUTA");
  Serial.println();

  acenderLED(255, 0, 0, TEMPO_ACESSO_NEGADO); // Red LED
}
