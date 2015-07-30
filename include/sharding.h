bool handle_shard_commands(PgSocket *client, PktHdr *pkt);
void find_shard(PgSocket *client, PktHdr *pkt);
regex_t sharding_command_regex;