/* A union may hold a struct that ends in an array with no size (C99
 * 6.7.2.1p2 forbids that struct only as a struct's member or an array's
 * element), which is how such a struct is given room to live in without
 * the heap. acc refused it as a member of anything. */
struct packet {
    int size;
    unsigned char data[];
};

static union {
    struct packet packet;
    unsigned char bytes[sizeof(struct packet) + 4];
} storage;

int main(void)
{
    struct packet *packet = &storage.packet;
    int total = 0;

    packet->size = 4;
    for (int i = 0; i < packet->size; i++)
        packet->data[i] = (unsigned char) (i + 9);
    for (int i = 0; i < packet->size; i++)
        total += packet->data[i];

    return total;                       /* 9 + 10 + 11 + 12 */
}
