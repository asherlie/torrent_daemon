/*
 * this daemon waits for two types of messages in two separate threads
 *
 * writes results to a directory that is periodically scanned by transmission daemon
 * this uses a struct diskmap to keep track of which torrents are accounted for
 *
 * diskmap[torrent_fn] = {type, deleted, torrent_data}
 *
 * files are deleted only when 
 *
 *
 * hmm, actually we don't really need a torrent tracker daemon, we can just have the tranmission daemon
 * this will directly wait for torrent notifications of both types
 * it will then insert these torrents into the diskmap, storing the actual data of the torrent file or magnet url in the diskmap struct
 *
 * TODO:
 *   before starting this i'd like to achieve the following:
 *      get diskmap to be a library that can be make installed and included with <>
 *      move localnotify to be a separate repo and do the same
 *      the torrent program should work with simple includes
 *
 *      potentially use docker containers - one with that one weird proxy, one with torrent software outlined here, USING that other
 *      proxy container as a --network adapter
 *
 *      and jellyfin will run on the metal
 *
 *      start_gluetun.sh will be run as a systemd service also that will be depended upon
 *
 *
 * on startup:
 *
 * init_diskmap()
 *
 * for i in diskmap:
 *  // hopefull there's a way to do this where transmission ignores it if already download(ing/ed)
 *  download torrent in transmission daemon
 *
 * while (1) {
 *  thread 0: get_magnet()
 *  thread 1: get_torrent_file()
 *
 *  thread 2: continuously broadcast torrent info for status bars
 *
 *  insert_diskmap(name, type, torrent_data)
 *
 *  <respond with confirmation of reception>
 *
 *  download torrent in transmission daemon
 *
 * }
 *
 * this will be a systemd daemon that restarts on failure
 * could also actually be a container that restarts on failure, how do i set this up?
 * use this: https://blog.container-solutions.com/running-docker-containers-with-systemd
 * seems pretty easy
 *
 * i can have this require/after the VPN container as well, should be simple - it'll just be a service using start_gluetun.sh
 * all will restart on failure and we'll have a good setup
 *
 * jellyfin can be natively configured on the pi
 *
 * once this is working it'll be as simple as the following from a machine on the network:
 *  ./jellyfin_dl torrent_info
 *
 * write a program that will work for linux, mac, and windows
 * also write in a feature to request download status! - there should be a status bar in the ./jellyfin_dl program
 *
 * ADD A THREAD IN THE DAEMON TO ALWAYS BROADCAST STATUS FOR ALL TORRENTS IN DISKMAP!!
 *
 * i can also get this all working on my thinkpad. even jellyfin should maybe just be in a container
 * this will allow very easy transition to raspberry pi
 *
 * look at ~/pi_copy/torrents_docker for previous work on this
 *
 * 3 containers:
 *  jellyfin
 *  VPN
 *  torrenter
 *
 *  ugh maybe i should rethink this whole design now that i know about the transmission-daemon -c flag that specifies a torrent directory to watch
 *  for new files
 *
 *  nope won't rethink
 *
 *  use podman to manage containers. there are instructions on jellyfin site about how to set this up. also use this for gluetun and torrent tracker
 *  https://jellyfin.org/docs/general/installation/container/
 *
 *  TODO: uninstall podman, docker on this machine once i get my new pi
 *
 */
#include <netinet/ip.h>
#include <fcntl.h>
#include <stdlib.h>
#include <localnotify.h>
#include <dm.h>

#define MAX_TORRENT_FILESZ 30000
#define MAX_TORRENT_NAMESZ 128
enum torrent_type {magnet_url = 0, torrent_file};
enum media_type {show = 0, movie};

struct torrent{
    char name[MAX_TORRENT_NAMESZ];
    enum torrent_type type;
    enum media_type mtype;
    uint8_t data[MAX_TORRENT_FILESZ];
};

struct enriched_torrent{
    struct torrent t;
    struct in_addr sender;
};

int hash_func(void* key, uint32_t keysz, uint32_t n_buckets) {
    if (keysz < sizeof(int)) {
        return 9 % n_buckets;
    }
    return *((int*)key) % n_buckets;
}

register_ln_payload(torrent, "wlp3s0", struct torrent, 253)

/* SERVER */

void start_torrent_dl(struct torrent* t) {
    char cmd[1024] = {0};
    char data_head[100];

    memcpy(data_head, t->data, 100);

    if (t->type == magnet_url) {
        snprintf(cmd, sizeof(cmd), "$ transmission-remote -a %s", data_head);
    } else {

        snprintf(cmd, sizeof(cmd), "transmission-remote -l");
    }
    printf("/* run: transmission-remote -> daemon : start_run type %i %s @ %s*/\n", t->type, t->name, data_head);
    puts(cmd);
    /*system();*/
}

void start_torrent_dl_iter(uint32_t keysz, void* key, uint32_t valsz, uint8_t* data) {
    (void)keysz;
    (void)valsz;
    (void)key;
    struct enriched_torrent* et = (struct enriched_torrent*)data;
    start_torrent_dl(&et->t);
}

// returns number of running torrents 
// need to implement a foreach function/macro in diskmap for this
// might be difficult, probabl not too bad though.
// i'll just foreach(torrents, start_torrent_dl())

int scan_and_start_torrent_dl(struct diskmap* torrents) {
    /*
     * void foreach_diskmap_const(struct diskmap* dm, uint32_t keysz, void (*funcptr)(uint32_t, void*, uint32_t, uint8_t*));
     * void foreach_diskmap(struct diskmap* dm, uint32_t keysz, void (*funcptr)(uint32_t, void*, uint32_t, uint8_t*));
    */
    foreach_diskmap(torrents, MAX_TORRENT_NAMESZ, start_torrent_dl_iter);
    return 0;
}

void check_status() {

/*
(cmd){100%}[asher]:[bell]@[01:06 PM] -> transmission-remote -l
    ID   Done       Have  ETA           Up    Down  Ratio  Status       Name
     1     2%   19.52 MB  20 min      11.0  1540.0   0.00  Up & Down    Zelig (1983) [1080p] [YTS.AG]
Sum:            19.52 MB              11.0  1540.0
*/
// need to parse this and return done % as well as ETA
//
// cat output  | while read line ; do echo $line | awk -F ' ' '{print $3}'; done
// can use this or maybe just filter by name 
//
// should i just leave it up to the user to use transmission-remote to connect to pi?
// no prob not

}

void server_main() {
    struct diskmap dm;
    _Bool success;
    struct enriched_torrent et;

    init_diskmap(&dm, 10, 1000, "TORRENTS", hash_func);

    /* ensure that all torrents are downloading */
    scan_and_start_torrent_dl(&dm);

    while (1) {
        // TODO: print sender IP
        et.t = recv_torrent(&success, &et.sender);
        if (!success) {
            continue;
        }

        // TODO: it's a waste of space to use name as a key and have it as a member
        // create a completely separate struct for enriched_torrent that doesn't have name!
        insert_diskmap(&dm, MAX_TORRENT_NAMESZ, sizeof(struct enriched_torrent), et.t.name, &et);
        /*void insert_diskmap(struct diskmap* dm, uint32_t keysz, uint32_t valsz, void* key, void* val);*/
        start_torrent_dl(&et.t);
    }
}

/* END SERVER */

/* CLIENT */

void client_main(char* label, uint8_t* torrent_data, int datalen, enum torrent_type type, enum media_type mtype) {
    struct torrent t;

    strncpy(t.name, label, sizeof(t.name) - 1);
    memcpy(t.data, torrent_data, datalen);
    t.type = type;
    t.mtype = mtype;
    broadcast_torrent(t);
}

/* END CLIENT */

int main(int a, char** b) {
    int fd;

    if (a == 3) {
        fd = open(b[2], O_RDONLY);
        // TODO: if fd is valid, read file and broadcast contents as a torrent_file
        if (fd == -1) {
            client_main(b[1], (uint8_t*)b[2], strlen(b[2]), magnet_url, movie);
        }
        close(fd);
        return 1;
    }
    server_main();
}
