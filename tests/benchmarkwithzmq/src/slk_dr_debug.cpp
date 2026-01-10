#include <serverlink/serverlink.h>
#include <thread>
#include <cstdio>
#include <cstring>
#include <vector>

void print_hex(const char* label, const char* buf, int len) {
    printf("%s (%d): ", label, len);
    for(int i=0; i<len; i++) printf("%02x ", (unsigned char)buf[i]);
    printf("| ");
    for(int i=0; i<len; i++) printf("%c", (buf[i] >= 32 && buf[i] <= 126) ? buf[i] : '.');
    printf("\n");
}

int main() {
    slk_ctx_t *ctx = slk_ctx_new();
    slk_socket_t *server = slk_socket(ctx, SLK_ROUTER);
    slk_socket_t *client = slk_socket(ctx, SLK_DEALER);

    slk_setsockopt(server, SLK_ROUTING_ID, "SRV", 3);
    slk_setsockopt(client, SLK_ROUTING_ID, "CLI", 3);

    slk_bind(server, "tcp://127.0.0.1:39001");
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    slk_connect(client, "tcp://127.0.0.1:39001");
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Dealer sends READY
    slk_send(client, "READY", 5, 0);

    // Router reads loop
    char buf[256];
    int rc = slk_recv(server, buf, 256, 0);
    print_hex("1st", buf, rc);
    
    rc = slk_recv(server, buf, 256, SLK_DONTWAIT);
    if (rc > 0) print_hex("2nd", buf, rc);
    else printf("2nd: EAGAIN\n");

    slk_close(client); slk_close(server); slk_ctx_destroy(ctx);
    return 0;
}
