#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/select.h>
#include <mosquitto.h>

#define BROKER "34.94.82.232"
#define PORT 1883
#define KEEPALIVE 15
#define TOPIC_SUB "terminalRequest"
#define TOPIC_PUB "espRequest"

void on_message(struct mosquitto *mosq, void *userdata, const struct mosquitto_message *msg);

struct mosquitto *mosq = NULL;
bool ai_mode = false;

// ---------- MQTT Publishing ----------
void publish_response(const char *message)
{
    mosquitto_publish(mosq, NULL, TOPIC_PUB, strlen(message) + 1, message, 0, false);
}

// ---------- Random AI Logic ----------
void aiX_play(char *board)
{
    int randNum = rand() % 9;
    while (board[randNum] != '_')
    {
        randNum = rand() % 9;
    }

    board[randNum] = 'X';
}

// ---------- Handle user input ----------
void handle_response_trigger()
{
    fd_set fds;
    struct timeval tv;
    char buffer[100];

    while (1) // loop until input is received
    {
        FD_ZERO(&fds);
        FD_SET(STDIN_FILENO, &fds);
        tv.tv_sec = 0;
        tv.tv_usec = 100000; // 0.1 sec

        int ret = select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv);
        if (ret > 0 && FD_ISSET(STDIN_FILENO, &fds))
        {
            int len = read(STDIN_FILENO, buffer, sizeof(buffer) - 1);
            if (len > 0)
            {
                buffer[len - 1] = '\0'; // trim newline
                publish_response(buffer);
                break; // exit loop after successful input
            }
        }
        // Otherwise, keep looping until input is ready
    }
}

// // ---------- AI Mode Message Callback ----------
// void ai_callback(struct mosquitto *mosq, void *userdata, const struct mosquitto_message *msg)
// {
//     printf("Callback switched to ai_callback\n");

//     if (msg->payloadlen)
//     {
//         printf("AI Callback: Received message on topic [%s]\n", msg->topic);
//         char message[msg->payloadlen + 1];
//         memcpy(message, msg->payload, msg->payloadlen);
//         message[msg->payloadlen] = '\0';

//         if (strcmp(message, "stop") == 0)
//         {
//             mosquitto_message_callback_set(mosq, on_message); // Exit AI mode
//         }
//         else if (strlen(message) == 9) // Heuristic: looks like a board
//         {
//             char board[10];
//             strncpy(board, message, 9);
//             board[9] = '\0';
//             aiX_play(board);
//             publish_response(board);
//         }
//         else
//         {
//             printf("%s\n", message);
//         }
//     }
// }

// ---------- Enter AI Mode ----------
// void ai_mode()
// {
//     printf("Entered AI mode\n");
//     mosquitto_message_callback_set(mosq, ai_callback);
//     mosquitto_unsubscribe(mosq, NULL, TOPIC_SUB);
//     mosquitto_subscribe(mosq, NULL, TOPIC_SUB, 0);

//     publish_response("ai_active");
// }

// ---------- Main Callback ----------
void on_message(struct mosquitto *mosq, void *userdata, const struct mosquitto_message *msg)
{
    if (msg->payloadlen)
    {
        char message[msg->payloadlen + 1];
        memcpy(message, msg->payload, msg->payloadlen);
        message[msg->payloadlen] = '\0';

        // Trim any trailing whitespace/newlines
        char *trimmed = message;
        while (isspace(trimmed[strlen(trimmed) - 1]))
        {
            trimmed[strlen(trimmed) - 1] = '\0';
        }
        if (ai_mode)
        {
            if (strcmp(message, "stop") == 0)
            {
                ai_mode = false; // Exit AI mode
            }
            else if (strlen(message) == 9) // Heuristic: looks like a board
            {
                char board[10];
                strncpy(board, message, 9);
                board[9] = '\0';
                aiX_play(board);
                publish_response(board);
            }
            else
            {
                printf("%s\n", message);
            }
        }
        else if (strcmp(trimmed, "response") == 0)
        {
            handle_response_trigger();
        }
        else if (strcmp(trimmed, "ai_mode") == 0)
        {
            ai_mode = true;
        }
        else
        {
            printf("%s\n", message);
        }
    }
}

// ---------- Main ----------
int main(int argc, char *argv[])
{
    srand(time(NULL));
    mosquitto_lib_init();
    mosq = mosquitto_new(NULL, true, NULL);

    if (!mosq)
    {
        fprintf(stderr, "Failed to create Mosquitto instance\n");
        return 1;
    }

    mosquitto_message_callback_set(mosq, on_message);
    int rc = mosquitto_connect(mosq, BROKER, PORT, KEEPALIVE);
    if (rc != 0)
    {
        fprintf(stderr, "Unable to connect to broker. Error Code: %d\n", rc);
        return 1;
    }

    mosquitto_subscribe(mosq, NULL, TOPIC_SUB, 0);

    publish_response("connected");

    while (1)
    {
        mosquitto_loop(mosq, -1, 1);
    }

    mosquitto_destroy(mosq);
    mosquitto_lib_cleanup();
    return 0;
}
