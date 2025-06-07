sleep(n_seconds);
for (i = 0; i < 2; i++) {
  ERecvStatus_t recv_status =
      recv_game_state(socket_context[i].sock, socket_context[i].set, &game_state[i],
                      &client_state[i], socket_context[i].id);
  assert(recv_status != RECV_ERROR);
  if (recv_status == RECV_NOTHING)
    fprintf(stderr, "Received nothing\n");
  assert(socket_context[i].sock != NULL);
}
