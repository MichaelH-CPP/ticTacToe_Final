#include <stdlib.h>
#include <time.h>
#include <LiquidCrystal_I2C.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <PubSubClient.h>
#include <Wire.h>

// WiFi and MQTT Broker Initiated
const char *ssid = "MHawara";
const char *password = "8186171911";
const char *mqtt_server = "34.94.82.232";

// Initialize the Clients
WiFiClient espClient;
PubSubClient client(espClient);

// Define the LCD Values
#define SDA 14 // Define SDA pins
#define SCL 13 // Define SCL pins
LiquidCrystal_I2C lcd(0x27, 16, 2);

// Function Prototypes
void singlePlayerMode();
void twoPlayerMode();
void aiMode();
void aiO(char *);
void aiX(char *);
void sendBoard(const char *, bool);
void player(char *, char);
char checkWinner(char *);

// Define Variables for Wins
int xWins = 0, oWins = 0, ties = 0;

// Define menu logic
String gameChoice;
char input[10] = "0";

/*

  [Connection Functions]:

  void connect_WiFi()
    - Connect to WiFi
  void reconnect()
    - If Esp32 gets disconnected, reconnect to the mqtt server
  bool connectToTerminal()
    - Connect to VSCode terminal, once it's connected, the powershell will
      broadcast a message announcing it's on the server and the ESP will respond
    - Returns a boolean once verified
  bool i2CAddrTest()
    - Connects to LCD screen
*/
void connect_WiFi()
{
    delay(100);
    Serial.begin(115200);
    while (!Serial)
        ;

    WiFi.begin(ssid, password);
    Serial.print("Connecting to WiFi");
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        Serial.print(".");
    }

    Serial.println("");
    Serial.print("WiFi Connected!\n IP Address: ");
    Serial.println(WiFi.localIP());
}
void reconnect()
{
    while (!client.connected())
    {
        Serial.print("Attempting MQTT connection...");
        if (client.connect("ESP32Client"))
        {
            Serial.println("Connected!");
            client.subscribe("espRequest");
        }
        else
        {
            Serial.print("failed, rc=");
            Serial.print(client.state());
            Serial.println("; Retrying in 3 seconds...");
            delay(3000);
        }
    }
}
void connect_terminal()
{
    Serial.print("Connecting to External Terminal");
    while (true)
    {
        client.loop();
        if (strcmp(input, "connected") != 0)
        {
            Serial.print(".");
            delay(500);
        }
        else
        {
            Serial.println();
            Serial.println("VSCode Connected!");
            strcpy(input, "in-game");
            break;
        }
    }
}
bool i2CAddrTest(uint8_t addr)
{
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0)
    {
        return true;
    }
    return false;
}

/*
  [Callback Functions] *all have same params
  void terminalCallback()
    - Verifies that information was received from the terminal
    - Publishes back to terminal to begin sending menu

*/
void terminalCallback(char *topic, byte *payload, unsigned int length)
{
    String message;
    for (int i = 0; i < length; i++)
    {
        message += (char)payload[i];
    }
    strcpy(input, "connected");
    client.publish("terminalRequest", input);
}

void menuCallback(char *topic, byte *payload, unsigned int length)
{
    Serial.print("Message arrived [");
    Serial.print(topic);
    Serial.print("] ");

    String message;
    for (int i = 0; i < length; i++)
    {
        message += (char)payload[i];
    }
    Serial.println(message);

    // Choose menu choice based on message
    switch (message[0])
    {
    case '1':
        singlePlayerMode();
        break;
    case '2':
        twoPlayerMode();
        break;
    case '3':
        aiMode();
        break;
    default:
        client.publish("terminalRequest", "Invalid Choice. Choose a number between 1-3.");
    }
}

void gameCallback(char *topic, byte *payload, unsigned int length)
{
    Serial.print("Message arrived [");
    Serial.print(topic);
    Serial.print("] ");

    memset(input, 0, sizeof(input));
    for (int i = 0; i < length && i < 9; i++)
    {
        input[i] = (char)payload[i];
    }
    input[9] = '\0';

    Serial.println(input);
}

/*
  [Player Functions]
  void aiO(char* board)
    - Modifies the board by adding an O

  void playerO(char* board)
    - Allows the user to place an O
    - If the user selects an invalid board, should republish the prompt with error

*/
void singlePlayerMode()
{
    char board[] = "_________";
    char winner = 'N';
    int start = rand() % 2;
    client.setCallback(gameCallback);
    strcpy(input, "0");

    client.publish("terminalRequest", "\n\nSingle-Player selected!\nYou will be Xs, I will be Os.\n");
    (start == 0) ? client.publish("terminalRequest", "I will start!") : client.publish("terminalRequest", "You will start!");
    delay(400);
    for (int i = 0; i < 9; i++)
    {
        // AI's Turn
        if (start % 2 == 0)
        {
            char board_msg[10];
            strncpy(board_msg, board, 9);
            board_msg[9] = '\0';
            client.publish("gcpRequest", board_msg);
            while (input[0] == '0')
            {
                client.loop();
            }
            strcpy(board, input);
            Serial.println(board);
            winner = checkWinner(board);
            strcpy(input, "0");
            if (winner != 'N')
                break;
            delay(500);
        }

        // Player's Turn
        else
        {
            client.publish("terminalRequest", "Your turn! Select any of the numbers!");
            sendBoard(board, false);
            player(board, 'X');
            winner = checkWinner(board);
            if (winner != 'N')
                break;
            delay(500);
        }
        start++;
    }

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Winner is:");
    lcd.setCursor(0, 1);
    switch (winner)
    {
    case 'N':
        ties++;
        lcd.print("None, tie!");
        client.publish("terminalRequest", "\nLooks like it was a tie! Take a look:");
        break;

    case 'O':
        oWins++;
        lcd.print("AI!");
        client.publish("terminalRequest", "\nBetter luck next time! See the results:");
        break;

    case 'X':
        xWins++;
        lcd.print("You!");
        client.publish("terminalRequest", "\nGreat job! Look at how you beat me:");
    }

    sendBoard(board, true);
    delay(2000);
}

void twoPlayerMode()
{
    char board[] = "_________";
    char winner = 'N';
    int start = rand() % 2;

    client.setCallback(gameCallback);

    client.publish("terminalRequest", "\n\nTwo-Player Mode selected!\n");
    (start == 0) ? client.publish("terminalRequest", "X will start!") : client.publish("terminalRequest", "O will start!");

    for (int i = 0; i < 9; i++)
    {
        if (start % 2 == 0)
        {
            client.publish("terminalRequest", "\nX's Turn; Select any of the numbers!");
            sendBoard(board, false);
            player(board, 'X');
            winner = checkWinner(board);
            if (winner != 'N')
                break;
            delay(500);
        }
        else
        {
            client.publish("terminalRequest", "\nO's turn; Select any of the numbers!");
            sendBoard(board, false);
            player(board, 'O');
            winner = checkWinner(board);
            if (winner != 'N')
                break;
            delay(500);
        }
        start++;
    }

    switch (winner)
    {
    case 'N':
        ties++;
        client.publish("terminalRequest", "\nLooks like it was a tie! Let's see it:");
        break;
    case 'O':
        oWins++;
        client.publish("terminalRequest", "\nThe winner is: O! See the results:");
        break;
    case 'X':
        xWins++;
        client.publish("terminalRequest", "\nThe winner is: X! Take a look:");
    }

    sendBoard(board, true);

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Winner is:");
    lcd.setCursor(0, 1);
    lcd.print(winner);

    delay(2000);
}

void aiMode()
{
    char board[] = "_________";
    char winner = 'N';
    int start = rand() % 2;

    client.setCallback(gameCallback);

    client.publish("terminalRequest", "AI Mode selected!");
    client.publish("terminalRequest", "ai_mode");
    delay(500);
    (start == 0) ? client.publish("terminalRequest", "\nX will start!") : client.publish("terminalRequest", "\nO will start!");

    for (int i = 0; i < 9; i++)
    {
        strcpy(input, "0");

        char board_msg[10];
        strncpy(board_msg, board, 9);
        board_msg[9] = '\0';

        if (start % 2 == 0)
        {
            // The Terminal's AI goes first
            client.publish("terminalRequest", "\nX's Turn!");
            client.publish("terminalRequest", board_msg);
        }
        else
        {
            client.publish("terminalRequest", "\nO's turn!");
            client.publish("gcpRequest", board_msg);
        }

        while (strcmp(input, "0") == 0)
        {
            client.loop();
        }
        strcpy(board, input);
        winner = checkWinner(board);
        strcpy(input, "0");
        if (winner != 'N')
            break;
        sendBoard(board, false);
        delay(1000);
        start++;
    }

    client.publish("terminalRequest", "stop");

    switch (winner)
    {
    case 'N':
        ties++;
        client.publish("terminalRequest", "Looks like it was a tie! Let's see it:");
        break;
    case 'O':
        oWins++;
        client.publish("terminalRequest", "The winner is: O! See the results:");
        break;
    case 'X':
        xWins++;
        client.publish("terminalRequest", "The winner is: X! Take a look:");
    }

    sendBoard(board, true);

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Winner is:");
    lcd.setCursor(0, 1);
    lcd.print(winner);

    delay(2000);
}

// void aiX(char* board) {
//   int randNum = rand() % 9;
//   while (board[randNum] != '_') {
//     randNum = rand() % 9;
//   }

//   board[randNum] = 'X';
// }

void player(char *board, char move)
{
    strcpy(input, "0");
    client.publish("terminalRequest", "response");
    while (true)
    {
        while (strcmp(input, "0") == 0)
        {
            client.loop();
            delay(100);
        }
        int position = atoi(input);
        if (position >= 1 && position <= 9 && board[position - 1] == '_')
        {
            board[position - 1] = move;
            strcpy(input, "0");
            break;
        }
        else
        {
            client.publish("terminalRequest", "Invalid spot. Choose another:");
            sendBoard(board, false);
        }
    }
}

char checkWinner(char *board)
{
    for (int i = 0; i < 9; i++)
    {
        if (board[i] != '_')
        {
            if (i < 3)
            {
                if (board[i] == board[i + 3] && board[i] == board[i + 6])
                {
                    return board[i];
                }
            }
        }

        if (i % 3 == 0)
        {
            if (board[i] != '_')
            {
                if (board[i] == board[i + 1] && board[i] == board[i + 2])
                {
                    return board[i];
                }
            }
        }
    }

    if (board[0] != '_' && board[0] == board[4] && board[0] == board[8])
    {
        return board[0];
    }
    else if (board[2] != '_' && board[2] == board[4] && board[2] == board[6])
    {
        return board[2];
    }

    return 'N';
}

void sendBoard(const char *board, bool isLast)
{
    char formattedBoard[300]; // Buffer for the formatted board
    char output[355];         // Buffer for the final output

    // Format the board with borders
    snprintf(formattedBoard, sizeof(formattedBoard),
             "%c | %c | %c\n"
             "---------\n"
             "%c | %c | %c\n"
             "---------\n"
             "%c | %c | %c",
             board[0], board[1], board[2],
             board[3], board[4], board[5],
             board[6], board[7], board[8]);

    if (!isLast)
    {
        // Create board with move numbers (1-9) in empty spaces
        char numberedBoard[300];
        snprintf(numberedBoard, sizeof(numberedBoard),
                 "%c | %c | %c\n"
                 "---------\n"
                 "%c | %c | %c\n"
                 "---------\n"
                 "%c | %c | %c",
                 (board[0] == '_') ? '1' : board[0],
                 (board[1] == '_') ? '2' : board[1],
                 (board[2] == '_') ? '3' : board[2],
                 (board[3] == '_') ? '4' : board[3],
                 (board[4] == '_') ? '5' : board[4],
                 (board[5] == '_') ? '6' : board[5],
                 (board[6] == '_') ? '7' : board[6],
                 (board[7] == '_') ? '8' : board[7],
                 (board[8] == '_') ? '9' : board[8]);

        // Combine both boards in the output
        snprintf(output, sizeof(output),
                 "Current Board:\n%s\n\n",
                 formattedBoard);
        client.publish("terminalRequest", output);
        snprintf(output, sizeof(output),
                 "Available Moves:\n%s",
                 numberedBoard);
        client.publish("terminalRequest", output);
    }
    else
    {
        // Just send the plain board
        snprintf(output, sizeof(output), "Final Board:\n%s", formattedBoard);
        client.publish("terminalRequest", output);
    }

    // Publish to MQTT topic
}

void setup()
{
    // Set up the LCD
    Wire.begin(SDA, SCL); // attach the IIC pin
    if (!i2CAddrTest(0x27))
    {
        lcd = LiquidCrystal_I2C(0x3F, 16, 2);
    }
    lcd.init();          // LCD driver initialization
    lcd.backlight();     // Open the backlight
    lcd.setCursor(0, 0); // Move the cursor to row 0, column 0

    // Connect to WiFi and Server
    connect_WiFi();
    client.setServer(mqtt_server, 1883);
    client.setCallback(terminalCallback);
    srand(time(NULL));
}

void loop()
{
    if (!client.connected())
    {
        reconnect();
        connect_terminal();
    }
    lcd.setCursor(0, 0);
    lcd.print("XWin: ");
    lcd.print(xWins);
    lcd.print(" Tie: ");
    lcd.print(ties);
    lcd.setCursor(0, 1);
    lcd.print("OWin: ");
    lcd.print(oWins);

    strcpy(input, "in-game");

    char menu[] = "Welcome to TicTacToe!\n1) Single-Player\n2) Two-Player\n3) Watch AIs";
    client.publish("terminalRequest", menu);
    client.setCallback(menuCallback);
    client.publish("terminalRequest", "response");
    while (strcmp(input, "in-game") == 0)
    {
        client.loop();
        delay(100);
    }
}
