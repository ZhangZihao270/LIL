#include "packet.h"

__thread common::FixedQueue<void>* PacketFactory::pkt_pool_ = NULL;
int64_t PacketFactory::pkt_size_ = sizeof(NullPacket);

void PacketFactory::init() {
  pkt_size_ = sizeof(NullPacket);
}
