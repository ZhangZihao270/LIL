//#include <assert.h>
//#include "packet_factory.h"
//#include "common/global.h"

/*
void StartPacket::wait() {
  service_->Requeststart(&ctx_, &request_, &responder_, cq_, cq_, this);
  STAT_INC(STAT_SERVER, START_COUNT, 1);
}

void PutPacket::wait() {
  service_->Requestput(&ctx_, &request_, &responder_, cq_, cq_, this);
  STAT_INC(STAT_SERVER, PUT_COUNT, 1);
}

void GetPacket::wait() {
  service_->Requestget(&ctx_, &request_, &responder_, cq_, cq_, this);
  STAT_INC(STAT_SERVER, GET_COUNT, 1);
}

void CommitPacket::wait() {
  service_->Requestcommit(&ctx_, &request_, &responder_, cq_, cq_, this);
  STAT_INC(STAT_SERVER, END_COUNT, 1);
}

Packet* PacketFactory::alloc(const Packet *pkt) {
  Packet* ret = NULL;
  void *ptr = NULL;
  switch(pkt->type_) {
    case PKT_START:
      ret = alloc<StartPacket>(pkt->service_, pkt->cq_);
      break;
    case PKT_PUT:
      ret = alloc<PutPacket>(pkt->service_, pkt->cq_);
      break;
    case PKT_GET:
      ret = alloc<GetPacket>(pkt->service_, pkt->cq_);
      break;
    case PKT_COMMIT:
      ret = alloc<CommitPacket>(pkt->service_, pkt->cq_);
      break;
    default:
      assert(0);
      break;
  }
  return ret;
}
*/
