#include "00_test.h"

_MAIN_HEAD_

struct player_t player = {0};
player = (struct player_t){.name = "Foo", .id = 1, .chips = 20000};

size_t size = 0;
uint8_t *data = serialize_player(&player, &size);

fprintf(stderr, "before conversion: %zu\n", size);
uint32_t size_net = htonl(size);

size = ntohl(size_net);
fprintf(stderr, "after conversion: %zu\n", size);

struct player_t player_receiver = deserialize_player(data, size);

free(data);

assert(strcmp(player_receiver.name, "Foo") == 0);
assert(player_receiver.id == 1);
assert(player_receiver.chips == 20000);

_MAIN_TAIL_
