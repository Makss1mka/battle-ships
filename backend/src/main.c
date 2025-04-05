#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <microhttpd.h>
#include <libwebsockets.h>
#include <jansson.h>
#include <pthread.h>

#define MAX_SESSIONS 100
#define BOARD_SIZE 10
#define MAX_SHIPS 10
#define MHD_MAX_JSON_SIZE 4096

static int callback_battleship(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len);


typedef enum {
    WAITING_FOR_PLAYER,
    IN_PROGRESS,
    FINISHED
} GameState;

typedef enum {
    EMPTY,
    SHIP,
    HIT,
    MISS
} CellState;

typedef struct {
    int x;
    int y;
} Point;

typedef struct {
    Point points[5]; // Максимальный корабль - 4 клетки
    int size;
    int hits;
} Ship;

typedef struct {
    CellState cells[BOARD_SIZE][BOARD_SIZE];
    Ship ships[MAX_SHIPS];
    int ship_count;
} Board;

typedef struct {
    char id[37];
    char player1[50];
    char player2[50];
    Board board1;
    Board board2;
    GameState state;
    int current_player;
    time_t created_at;
} GameSession;

typedef struct {
    GameSession sessions[MAX_SESSIONS];
    int session_count;
    pthread_mutex_t mutex;
} ServerState;

ServerState server_state;

void generate_uuid(char *uuid) {
    const char *chars = "0123456789abcdef";
    for (int i = 0; i < 36; i++) {
        if (i == 8 || i == 13 || i == 18 || i == 23) {
            uuid[i] = '-';
        } else {
            uuid[i] = chars[rand() % 16];
        }
    }
    uuid[36] = '\0';
}

void init_board(Board *board) {
    memset(board, 0, sizeof(Board));
    for (int i = 0; i < BOARD_SIZE; i++) {
        for (int j = 0; j < BOARD_SIZE; j++) {
            board->cells[i][j] = EMPTY;
        }
    }
    board->ship_count = 0;
}

int can_place_ship(Board *board, int x, int y, int size, int horizontal) {
    if (horizontal) {
        if (x + size > BOARD_SIZE) return 0;
        for (int i = x - 1; i <= x + size; i++) {
            for (int j = y - 1; j <= y + 1; j++) {
                if (i >= 0 && i < BOARD_SIZE && j >= 0 && j < BOARD_SIZE) {
                    if (board->cells[j][i] == SHIP) return 0;
                }
            }
        }
    } else {
        if (y + size > BOARD_SIZE) return 0;
        for (int i = x - 1; i <= x + 1; i++) {
            for (int j = y - 1; j <= y + size; j++) {
                if (i >= 0 && i < BOARD_SIZE && j >= 0 && j < BOARD_SIZE) {
                    if (board->cells[j][i] == SHIP) return 0;
                }
            }
        }
    }
    return 1;
}

void place_ship(Board *board, int x, int y, int size, int horizontal) {
    Ship *ship = &board->ships[board->ship_count++];
    ship->size = size;
    ship->hits = 0;
    
    if (horizontal) {
        for (int i = 0; i < size; i++) {
            board->cells[y][x + i] = SHIP;
            ship->points[i].x = x + i;
            ship->points[i].y = y;
        }
    } else {
        for (int i = 0; i < size; i++) {
            board->cells[y + i][x] = SHIP;
            ship->points[i].x = x;
            ship->points[i].y = y + i;
        }
    }
}

void setup_random_board(Board *board) {
    init_board(board);
    
    int sizes[] = {4, 3, 3, 2, 2, 2, 1, 1, 1, 1};
    int ship_count = sizeof(sizes) / sizeof(int);
    
    for (int i = 0; i < ship_count; i++) {
        int placed = 0;
        while (!placed) {
            int x = rand() % BOARD_SIZE;
            int y = rand() % BOARD_SIZE;
            int horizontal = rand() % 2;
            
            if (can_place_ship(board, x, y, sizes[i], horizontal)) {
                place_ship(board, x, y, sizes[i], horizontal);
                placed = 1;
            }
        }
    }
}

int check_hit(Board *board, int x, int y) {
    if (x < 0 || x >= BOARD_SIZE || y < 0 || y >= BOARD_SIZE) {
        return 0;
    }
    
    if (board->cells[y][x] == SHIP) {
        board->cells[y][x] = HIT;
        
        // Находим корабль и увеличиваем счетчик попаданий
        for (int i = 0; i < board->ship_count; i++) {
            for (int j = 0; j < board->ships[i].size; j++) {
                if (board->ships[i].points[j].x == x && board->ships[i].points[j].y == y) {
                    board->ships[i].hits++;
                    return 1;
                }
            }
        }
    } else if (board->cells[y][x] == EMPTY) {
        board->cells[y][x] = MISS;
    }
    
    return 0;
}

int is_ship_sunk(Board *board, int x, int y) {
    for (int i = 0; i < board->ship_count; i++) {
        for (int j = 0; j < board->ships[i].size; j++) {
            if (board->ships[i].points[j].x == x && board->ships[i].points[j].y == y) {
                return board->ships[i].hits == board->ships[i].size;
            }
        }
    }
    return 0;
}

int is_game_over(Board *board) {
    for (int i = 0; i < board->ship_count; i++) {
        if (board->ships[i].hits < board->ships[i].size) {
            return 0;
        }
    }
    return 1;
}




GameSession* create_session(const char *player_name) {
    if (server_state.session_count >= MAX_SESSIONS) {
        return NULL;
    }
    
    GameSession *session = &server_state.sessions[server_state.session_count++];
    generate_uuid(session->id);
    strncpy(session->player1, player_name, sizeof(session->player1) - 1);
    session->player2[0] = '\0';
    
    setup_random_board(&session->board1);
    init_board(&session->board2);
    
    session->state = WAITING_FOR_PLAYER;
    session->current_player = 1;
    session->created_at = time(NULL);
    
    return session;
}

GameSession* find_session(const char *session_id) {
    for (int i = 0; i < server_state.session_count; i++) {
        if (strcmp(server_state.sessions[i].id, session_id) == 0) {
            return &server_state.sessions[i];
        }
    }
    return NULL;
}

int join_session(GameSession *session, const char *player_name) {
    if (session->state != WAITING_FOR_PLAYER) {
        return 0;
    }
    
    strncpy(session->player2, player_name, sizeof(session->player2) - 1);
    setup_random_board(&session->board2);
    session->state = IN_PROGRESS;
    return 1;
}



static struct lws_protocols protocols[] = {
    {
        "battleship-protocol",
        callback_battleship,
        0,
        4096,
        0, NULL, 0
    },
    {NULL, NULL, 0, 0, 0, NULL, 0 } // Завершающий элемент
};

/*
    wsi - ptr on websocket connection 
    reason - reason of call
    user - user data
    in - input data
    len - input data len
*/
static int callback_battleship(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len) {
    (void)user;

    switch (reason) {
        case LWS_CALLBACK_ESTABLISHED:
            printf("WebSocket connection established\n");
            break;

        case LWS_CALLBACK_RECEIVE: {
            char *message = (char*)in;
            printf("Received message: %.*s\n", (int)len, message);

            json_error_t error;
            json_t *root = json_loadb(message, len, 0, &error);

            if (!root) {
                printf("JSON parse error: %s\n", error.text);
                break;
            }

            json_t *type_json = json_object_get(root, "type");

            if (!json_is_string(type_json)) {
                json_decref(root);
                break;
            }

            const char *type = json_string_value(type_json);
            if (strcmp(type, "attack") == 0) {
                json_t *session_id_json = json_object_get(root, "session_id");
                json_t *x_json = json_object_get(root, "x");
                json_t *y_json = json_object_get(root, "y");

                if (!json_is_string(session_id_json) || !json_is_integer(x_json) || !json_is_integer(y_json)) {
                    json_decref(root);
                    break;
                }

                const char *session_id = json_string_value(session_id_json);
                int x = json_integer_value(x_json);
                int y = json_integer_value(y_json);

                pthread_mutex_lock(&server_state.mutex);
                GameSession *session = find_session(session_id);

                if (!session || session->state != IN_PROGRESS) {
                    pthread_mutex_unlock(&server_state.mutex);
                    json_decref(root);
                    break;
                }

                const char *player_name = (const char *)lws_wsi_user(wsi);
                int is_player1 = player_name && strcmp(player_name, session->player1) == 0;
                int is_player2 = player_name && strcmp(player_name, session->player2) == 0;

                if ((session->current_player == 1 && !is_player1) || (session->current_player == 2 && !is_player2)) {
                    pthread_mutex_unlock(&server_state.mutex);
                    json_decref(root);
                    break;
                }

                Board *target_board = (session->current_player == 1) ? &session->board2 : &session->board1;
                int hit = check_hit(target_board, x, y);
                int sunk = 0;
                
                if (hit) {
                    sunk = is_ship_sunk(target_board, x, y);
                }

                int game_over = is_game_over(target_board);
                
                if (game_over) {
                    session->state = FINISHED;
                } else if (!hit) {
                    session->current_player = (session->current_player == 1) ? 2 : 1;
                }

                json_t *response = json_object();
                json_object_set_new(response, "type", json_string("attack_result"));
                json_object_set_new(response, "x", json_integer(x));
                json_object_set_new(response, "y", json_integer(y));
                json_object_set_new(response, "hit", json_boolean(hit));
                json_object_set_new(response, "sunk", json_boolean(sunk));
                json_object_set_new(response, "game_over", json_boolean(game_over));
                json_object_set_new(response, "next_player", json_integer(session->current_player));

                char *response_str = json_dumps(response, JSON_COMPACT);
                json_decref(response);
                pthread_mutex_unlock(&server_state.mutex);

                lws_callback_on_writable_all_protocol(lws_get_context(wsi), &protocols[0]);
                free(response_str);
            }

            json_decref(root);
            break;
        }

        case LWS_CALLBACK_CLOSED:
            printf("WebSocket connection closed\n");
            break;

        default:
            break;
    }

    return 0;
}



int send_error(struct MHD_Connection *connection, const char *message, int status_code) {
    json_t *error = json_object();
    json_object_set_new(error, "error", json_string(message));

    char *error_str = json_dumps(error, JSON_COMPACT);
    json_decref(error);

    struct MHD_Response *response = MHD_create_response_from_buffer(strlen(error_str), error_str, MHD_RESPMEM_MUST_FREE);
    
    int ret = MHD_queue_response(connection, status_code, response);
    MHD_destroy_response(response);

    return ret;
}

int handle_join_session(struct MHD_Connection *connection, const char *upload_data, size_t *upload_data_size) {
    if (*upload_data_size > MHD_MAX_JSON_SIZE) {
        return send_error(connection, "Payload too large", MHD_HTTP_CONTENT_TOO_LARGE);
    }

    json_error_t error;
    json_t *root = json_loadb(upload_data, *upload_data_size, 0, &error);
    if (!root) {
        return send_error(connection, "Invalid JSON", MHD_HTTP_BAD_REQUEST);
    }

    json_t *session_id_json = json_object_get(root, "session_id");
    json_t *player_name_json = json_object_get(root, "player_name");

    if (!json_is_string(session_id_json) || !json_is_string(player_name_json)) {
        json_decref(root);
        return send_error(connection, "Missing fields", MHD_HTTP_BAD_REQUEST);
    }

    const char *session_id = json_string_value(session_id_json);
    const char *player_name = json_string_value(player_name_json);

    pthread_mutex_lock(&server_state.mutex);
    GameSession *session = find_session(session_id);
    int is_player_joined = session ? join_session(session, player_name) : 0;
    pthread_mutex_unlock(&server_state.mutex);

    if (!is_player_joined) {
        json_decref(root);
        return send_error(connection, "Cannot join session", MHD_HTTP_BAD_REQUEST);
    }

    json_t *response = json_object();
    json_object_set_new(response, "session_id", json_string(session->id));
    json_object_set_new(response, "player", json_string("player2"));

    char *response_str = json_dumps(response, JSON_COMPACT);
    json_decref(response);
    json_decref(root);

    struct MHD_Response *mhd_response = MHD_create_response_from_buffer(strlen(response_str), response_str, MHD_RESPMEM_MUST_FREE);
    MHD_add_response_header(mhd_response, "Content-Type", "application/json");
    MHD_add_response_header(mhd_response, "Connection", "keep-alive");

    int ret = MHD_queue_response(connection, MHD_HTTP_OK, mhd_response);
    MHD_destroy_response(mhd_response);

    return ret;
}

int handle_create_session(struct MHD_Connection *connection, const char *upload_data, size_t *upload_data_size) {
    if (*upload_data_size > MHD_MAX_JSON_SIZE) {
        return send_error(connection, "Payload too large", MHD_HTTP_CONTENT_TOO_LARGE);
    }

    json_error_t error;
    json_t *root = json_loadb(upload_data, *upload_data_size, 0, &error);
    if (!root) {
        return send_error(connection, "Invalid JSON", MHD_HTTP_BAD_REQUEST);
    }

    json_t *player_name_json = json_object_get(root, "player_name");
    if (!json_is_string(player_name_json)) {
        json_decref(root);
        return send_error(connection, "Missing player_name", MHD_HTTP_BAD_REQUEST);
    }

    const char *player_name = json_string_value(player_name_json);

    pthread_mutex_lock(&server_state.mutex);
    GameSession *session = create_session(player_name);
    pthread_mutex_unlock(&server_state.mutex);

    if (!session) {
        json_decref(root);
        return send_error(connection, "Max sessions reached", MHD_HTTP_SERVICE_UNAVAILABLE);
    }

    json_t *response = json_object();
    json_object_set_new(response, "session_id", json_string(session->id));
    json_object_set_new(response, "player", json_string("player1"));

    char *response_str = json_dumps(response, JSON_COMPACT);
    json_decref(response);
    json_decref(root);

    struct MHD_Response *mhd_response = MHD_create_response_from_buffer(strlen(response_str), response_str, MHD_RESPMEM_MUST_FREE);
    MHD_add_response_header(mhd_response, "Content-Type", "application/json");
    MHD_add_response_header(mhd_response, "Connection", "keep-alive");

    printf("\nResponse string: %s\n %zu", response_str, strlen(response_str));

    int ret = MHD_queue_response(connection, MHD_HTTP_OK, mhd_response);

    printf("\nret - %d", ret); 

    MHD_destroy_response(mhd_response);
    
    return ret;
}

int handle_list_sessions(struct MHD_Connection *connection) {
    pthread_mutex_lock(&server_state.mutex);

    json_t *sessions_array = json_array();
    for (int i = 0; i < server_state.session_count; i++) {
        GameSession *session = &server_state.sessions[i];
        if (session->state == WAITING_FOR_PLAYER) {
            json_t *session_obj = json_object();
            json_object_set_new(session_obj, "id", json_string(session->id));
            json_object_set_new(session_obj, "player1", json_string(session->player1));
            json_object_set_new(session_obj, "created_at", json_integer(session->created_at));
            json_array_append_new(sessions_array, session_obj);
        }
    }

    pthread_mutex_unlock(&server_state.mutex);

    char *response_str = json_dumps(sessions_array, JSON_COMPACT);
    json_decref(sessions_array);

    struct MHD_Response *mhd_response = MHD_create_response_from_buffer(strlen(response_str), response_str, MHD_RESPMEM_MUST_FREE);
    MHD_add_response_header(mhd_response, "Content-Type", "application/json");

    int ret = MHD_queue_response(connection, MHD_HTTP_OK, mhd_response);
    MHD_destroy_response(mhd_response);

    return ret;
}


/*
    PARAMS:
    cls - user data
    connection - current HTTP connection
    url - url
    method - request method
    version - HTTP version
    upload_data - request body
    upload_data_size - request body size
    con_cls - data about connect
*/
enum MHD_Result http_handler(void *cls, struct MHD_Connection *connection,
    const char *url, const char *method,
    const char *version, const char *upload_data,
    size_t *upload_data_size, void **con_cls) {
    (void)cls;
    (void)version;

    if (*con_cls == NULL) {
        *con_cls = (void*)1;
        printf("\nALARM 0");
        return MHD_YES;
    }
    
    if (strcmp(method, "POST") == 0) {
        if (*upload_data_size != 0) {
            *upload_data_size = 0;
            return MHD_YES;
        }
    }

    if (strcmp(url, "/create") == 0 && strcmp(method, "POST") == 0) {
        return handle_create_session(connection, upload_data, upload_data_size);
    } else if (strcmp(url, "/join") == 0 && strcmp(method, "POST") == 0) {
        return handle_join_session(connection, upload_data, upload_data_size);
    } else if (strcmp(url, "/sessions") == 0 && strcmp(method, "GET") == 0) {
        return handle_list_sessions(connection);
    }

    return send_error(connection, "Not Found", MHD_HTTP_NOT_FOUND);;
}




void connection_done(void *cls, struct MHD_Connection *connection, void **con_cls, enum MHD_RequestTerminationCode code) {
    printf("\nСоединение закрыто, код завершения: %d\n", code);
}

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    srand(time(NULL));

    pthread_mutex_init(&server_state.mutex, NULL);
    server_state.session_count = 0;
    
    // HTTP сервер
    // struct MHD_Daemon *http_daemon = MHD_start_daemon(
    //     //MHD_USE_SELECT_INTERNALLY, 
    //     MHD_USE_THREAD_PER_CONNECTION,
    //     8080, 
    //     NULL, 
    //     NULL,
    //     &http_handler, 
    //     NULL, 
    //     MHD_OPTION_END
    // );
    struct MHD_Daemon *http_daemon = MHD_start_daemon(
        MHD_USE_THREAD_PER_CONNECTION, 
        8080, 
        NULL, 
        NULL, 
        &http_handler, 
        NULL, 
        MHD_OPTION_CONNECTION_TIMEOUT, 
        10, 
        MHD_OPTION_NOTIFY_COMPLETED, 
        &connection_done, 
        NULL, 
        MHD_OPTION_END
    );



    if (!http_daemon) {
        fprintf(stderr, "Failed to start HTTP server\n");
        return 1;
    }
    
    // WebSocket сервер
    struct lws_context_creation_info info;
    memset(&info, 0, sizeof(info));
    
    info.port = 9000;
    info.protocols = protocols;
    info.gid = -1;
    info.uid = -1;
    
    struct lws_context *context = lws_create_context(&info);
    if (!context) {
        fprintf(stderr, "Failed to create WebSocket context\n");
        MHD_stop_daemon(http_daemon);
        return 1;
    }
    
    printf("Server started. HTTP on port 8080, WebSockets on port 9000\n");
    
    while (1) {
        MHD_run(http_daemon);
        lws_service(context, 50);
    }
    
    lws_context_destroy(context);
    MHD_stop_daemon(http_daemon);
    pthread_mutex_destroy(&server_state.mutex);
    return 0;
}


