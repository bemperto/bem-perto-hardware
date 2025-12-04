// Aumentar buffer Serial ANTES de qualquer include
#define SERIAL_RX_BUFFER_SIZE 256

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
#define KEEPALIVE_INTERVAL 5000 // Send keepalive every 5 seconds

MFRC522 mfrc522(SS_PIN, RST_PIN);

// Estados do sistema para autorização via backend
bool aguardandoResposta = false;
unsigned long tempoRequisicao = 0;
const unsigned long TIMEOUT_MS = 5000;
const unsigned long DEBOUNCE_MS = 2000;
unsigned long ultimoScan = 0;
unsigned long ultimoKeepalive = 0;
String uidPendente = "";

void configurarLEDs();
void piscarAzul(int vezes);
void acenderLED(byte r, byte g, byte b, int tempo);
void desligarLEDs();
String lerUID();
void enviarRequisicaoAuth(String uid);
void processarRespostaAuth(String linha);
void verificarTimeout();
void mostrarAguardando();
void concederAcessoSimples(String nome);
void negarAcessoComRazao(String razao);
void negarAcessoPorTimeout();
void processarComandoTexto(String comando);

void setup()
{
  Serial.begin(9600); // Hardware Serial for Bluetooth (HC-05)
  SPI.begin();
  mfrc522.PCD_Init();

  configurarLEDs();

  // Wait for Serial to stabilize
  delay(100);

  Serial.println("{\"status\":\"ready\",\"message\":\"Sistema iniciado\"}");
}

void loop()
{
  // PRIORIDADE: Ler respostas do backend PRIMEIRO
  while (Serial.available())
  {
    String linha = Serial.readStringUntil('\n');
    linha.trim();

    // DEBUG: Mostrar tudo que recebe
    if (linha.length() > 0)
    {
      Serial.print("[ARDUINO DEBUG] Recebido: ");
      Serial.println(linha);
    }

    // Verificar se é resposta de auth
    if (linha.indexOf("\"type\":\"auth_response\"") >= 0)
    {
      Serial.println("[ARDUINO DEBUG] Detectada resposta de auth!");
      processarRespostaAuth(linha);
    }
    // Processar comandos antigos (PING, STATUS, etc)
    else if (linha.length() > 0 && linha.charAt(0) != '{')
    {
      processarComandoTexto(linha);
    }
  }

  // 2. Verificar timeout
  verificarTimeout();

  // 3. Mostrar LED de aguardando se esperando resposta (sem bloquear!)
  if (aguardandoResposta)
  {
    mostrarAguardando();
  }

  // 4. Enviar keepalive
  unsigned long agora = millis();
  if (agora - ultimoKeepalive >= KEEPALIVE_INTERVAL)
  {
    Serial.println("{\"type\":\"keepalive\",\"uptime\":" + String(agora) + "}");
    ultimoKeepalive = agora;
  }

  // 5. Verificar novo cartão
  if (aguardandoResposta)
  {
    return; // Não processar novos cartões enquanto aguarda
  }

  // Debounce: ignorar scans muito rápidos
  if (millis() - ultimoScan < DEBOUNCE_MS)
  {
    return;
  }

  if (!mfrc522.PICC_IsNewCardPresent())
  {
    return;
  }

  if (!mfrc522.PICC_ReadCardSerial())
  {
    return;
  }

  String uid = lerUID();
  ultimoScan = millis();

  piscarAzul(VEZES_BLINK);
  enviarRequisicaoAuth(uid);
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

// Nova função: Enviar requisição de autorização
void enviarRequisicaoAuth(String uid)
{
  Serial.print("{\"type\":\"auth_request\",\"uid\":\"");
  Serial.print(uid);
  Serial.print("\",\"timestamp\":");
  Serial.print(millis());
  Serial.println("}");

  // Pequeno delay para garantir envio
  delay(50);

  aguardandoResposta = true;
  tempoRequisicao = millis();
  uidPendente = uid;
}

// Nova função: Processar resposta de autorização
void processarRespostaAuth(String linha)
{
  Serial.println("[ARDUINO DEBUG] Iniciando parse da resposta");

  // Parse JSON manualmente (evitar biblioteca pesada)
  int typePos = linha.indexOf("\"type\":\"auth_response\"");
  if (typePos < 0)
  {
    Serial.println("[ARDUINO DEBUG] ERRO: type não encontrado!");
    return;
  }

  int accessPos = linha.indexOf("\"access\":");
  if (accessPos < 0)
  {
    Serial.println("[ARDUINO DEBUG] ERRO: access não encontrado!");
    return;
  }

  String accessStr = linha.substring(accessPos + 9, accessPos + 13);
  bool access = accessStr == "true";

  Serial.print("[ARDUINO DEBUG] Access string: '");
  Serial.print(accessStr);
  Serial.print("' -> access = ");
  Serial.println(access ? "TRUE" : "FALSE");

  aguardandoResposta = false;

  if (access)
  {
    // Extrair nome (opcional)
    int nameStart = linha.indexOf("\"name\":\"");
    String nome = "Autorizado";
    if (nameStart >= 0)
    {
      int nameEnd = linha.indexOf("\"", nameStart + 8);
      nome = linha.substring(nameStart + 8, nameEnd);
    }
    Serial.print("[ARDUINO DEBUG] Concedendo acesso para: ");
    Serial.println(nome);
    concederAcessoSimples(nome);
  }
  else
  {
    // Extrair razão (opcional)
    int reasonStart = linha.indexOf("\"reason\":\"");
    String razao = "negado";
    if (reasonStart >= 0)
    {
      int reasonEnd = linha.indexOf("\"", reasonStart + 10);
      razao = linha.substring(reasonStart + 10, reasonEnd);
    }
    Serial.print("[ARDUINO DEBUG] Negando acesso: ");
    Serial.println(razao);
    negarAcessoComRazao(razao);
  }
}

// Nova função: Verificar timeout
void verificarTimeout()
{
  if (!aguardandoResposta)
    return;

  if (millis() - tempoRequisicao >= TIMEOUT_MS)
  {
    aguardandoResposta = false;

    // FAIL-SECURE: Negar acesso
    Serial.println("TIMEOUT: Backend não respondeu");
    negarAcessoPorTimeout();
  }
}

// Nova função: Mostrar LED aguardando
void mostrarAguardando()
{
  // Piscar LED azul/amarelo enquanto aguarda
  unsigned long agora = millis();
  if ((agora / 200) % 2 == 0)
  {
    digitalWrite(LED_AZUL, HIGH);
  }
  else
  {
    digitalWrite(LED_AZUL, LOW);
  }
}

// Nova função: Conceder acesso simplificado
void concederAcessoSimples(String nome)
{
  // LED verde piscando 3x
  for (int i = 0; i < 3; i++)
  {
    digitalWrite(LED_VERDE, HIGH);
    delay(TEMPO_BLINK);
    digitalWrite(LED_VERDE, LOW);
    delay(TEMPO_BLINK);
  }
  digitalWrite(LED_VERDE, HIGH);
  delay(TEMPO_ACESSO_LIBERADO);
  desligarLEDs();
}

// Nova função: Negar acesso com razão
void negarAcessoComRazao(String razao)
{
  // LED vermelho sólido
  digitalWrite(LED_VERMELHO, HIGH);
  delay(TEMPO_ACESSO_NEGADO);
  desligarLEDs();
}

// Nova função: Negar acesso por timeout
void negarAcessoPorTimeout()
{
  // LED vermelho piscando rápido (5x)
  for (int i = 0; i < 5; i++)
  {
    digitalWrite(LED_VERMELHO, HIGH);
    delay(100);
    digitalWrite(LED_VERMELHO, LOW);
    delay(100);
  }
}

// Process incoming text commands (PING, STATUS)
void processarComandoTexto(String comando)
{
  comando.trim();

  // Command format: PING
  if (comando == "PING")
  {
    Serial.println("{\"status\":\"ok\",\"message\":\"PONG\"}");
  }
  // Command format: STATUS
  else if (comando == "STATUS")
  {
    Serial.print("{\"status\":\"ok\",\"uptime\":");
    Serial.print(millis());
    Serial.println("}");
  }
  else
  {
    Serial.println("{\"status\":\"error\",\"message\":\"Comando desconhecido\"}");
  }
}
