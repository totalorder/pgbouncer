#include <bouncer.h>

bool parse_shard_command(PgSocket *client, char *query);
int tokenize_shard_command(char *cursor, char **cluster_name, char **sharding_key, char **remaining_query);

bool query_equals(char *query, char *other_query) {
    return strcasecmp(query, other_query) == 0;
}

void find_shard(PgSocket *client, PktHdr *pkt) {
    if (pkt->type == 'Q') {
        char *query;
        if (!mbuf_get_string(&pkt->data, (const char **) &query)) {
            return;
        }
        pkt->data.read_pos = 0;

        char *cluster_name = NULL;
        if (client->cluster != NULL) {
            cluster_name = client->cluster->name;
        }
        log_debug("(cluster %s, key %s) query: %s", cluster_name, client->sharding_key, query);
    }
}

bool handle_shard_commands(PgSocket *client, PktHdr *pkt) {
    if (!client->sbuf.processed) {
        client->sbuf.processed = true;
        if (pkt->type == 'Q') {
            char *query;
            if (!mbuf_get_string(&pkt->data, (const char**)&query)) {
                log_debug("got incomplete client query!");
                return false;
            }
            pkt->data.read_pos = 0;

            if (!client->sharding_initialized) {
                if (query_equals(query, "SHARDINIT;")) {
                    log_debug("Evt: SHARDINIT");
                    client->sharding_initialized = true;

                    admin_ready(client, "shard commands are now accepted");
                    sbuf_prepare_skip(&client->sbuf, pkt->len);
                    return true;
                }
            } else {
                if (client->tx_state == TX_NONE) {
                    if (client->sharding_key != NULL) {
                        free(client->sharding_key);
                        client->sharding_key = NULL;
                        client->cluster = NULL;
                    }
                }

                if(!parse_shard_command(client, query)) {
                    return false;
                } else {
                    log_debug("Evt: TX_SHARD_INFO_SET");
                    client->tx_state = TX_SHARD_INFO_SET;
                    admin_ready(client, "shard command successful");
                    sbuf_prepare_skip(&client->sbuf, pkt->len);

                    return true;
                }
            }
        }
    }
    return false;
}

bool parse_shard_command(PgSocket *client, char *query) {
    char *cluster_name = NULL;
    char *sharding_key = NULL;
    char *remaining_query = NULL;

    if (strncasecmp(query, "shard ", 6) == 0) {
        if (tokenize_shard_command(query + 6, &cluster_name, &sharding_key, &remaining_query) == 4) {
            log_debug("got cluster name: %s", cluster_name);
            log_debug("got sharding key: %s", sharding_key);

            PgCluster *cluster = find_cluster(cluster_name);
            if (cluster == NULL) {
                admin_error(client, "Invalid cluster: %s", cluster_name);
                disconnect_client(client, true, "invalid cluster");
                return false;
            }

            if (client->sharding_key != NULL) {
                free(client->sharding_key);
            }

            client->sharding_key = sharding_key;
            client->cluster = cluster;
            return true;
        }
        log_debug("cmp3");
    }
    return false;
};

int tokenize_shard_command(char *cursor, char **cluster_name, char **sharding_key, char **remaining_query) {
    unsigned int match_idx;
    unsigned int max_matches = 1;
    unsigned int matches = 0;
    unsigned int max_groups = 4;
    unsigned int groups = 0;
    regmatch_t group_array[max_groups];

    for (match_idx = 0; match_idx < max_matches + 1; match_idx++) {
        unsigned int group_idx = 0;
        int offset = 0;

        if (regexec(&sharding_command_regex, cursor, max_groups, group_array, 0)) {
            break;  // No more matches
        }

        matches++;

        for (group_idx = 0; group_idx < max_groups; group_idx++)
        {
            if (group_array[group_idx].rm_so == (size_t)-1) {
                break;  // No more groups
            }

            groups++;

            if (group_idx == 0) {
                offset = group_array[group_idx].rm_eo;
            }

            if (group_idx == 1) {
                char *cursor_copy = strdup(cursor + group_array[group_idx].rm_so);
                cursor_copy[group_array[group_idx].rm_eo - group_array[group_idx].rm_so] = 0;

                *cluster_name = cursor_copy;
            } else if (group_idx == 2) {
                char *cursor_copy = strdup(cursor + group_array[group_idx].rm_so);
                cursor_copy[group_array[group_idx].rm_eo - group_array[group_idx].rm_so] = 0;

                *sharding_key = cursor_copy;
            } else if (group_idx == 3) {
                *remaining_query = (char *)(cursor + group_array[group_idx].rm_so);
            }
        }
        cursor += offset;
    }

    if (matches > max_matches) {
        return -1;
    }
    return groups;
}