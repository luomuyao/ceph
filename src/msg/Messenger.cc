// -*- mode:C++; tab-width:8; c-basic-offset:2; indent-tabs-mode:t -*-
// vim: ts=8 sw=2 smarttab

#include <random>
#include "include/Spinlock.h"
#include "include/types.h"
#include "Messenger.h"

#include "msg/simple/SimpleMessenger.h"
#include "msg/async/AsyncMessenger.h"
#include "msg/direct/DirectMessenger.h"
#include "msg/QueueStrategy.h"
#ifdef HAVE_XIO
#include "msg/xio/XioMessenger.h"
#endif

Messenger *Messenger::create_client_messenger(CephContext *cct, string lname, MessageFactory *factory)
{
  uint64_t nonce = 0;
  get_random_bytes((char*)&nonce, sizeof(nonce));
  return Messenger::create(cct, cct->_conf->ms_type, entity_name_t::CLIENT(),
			   lname, nonce, 0, factory);
}

Messenger *Messenger::create(CephContext *cct, const string &type,
			     entity_name_t name, string lname,
			     uint64_t nonce, uint64_t cflags,
			     MessageFactory *factory)
{
  int r = -1;
  if (type == "random") {
    static std::random_device seed;
    static std::default_random_engine random_engine(seed());
    static Spinlock random_lock;

    std::lock_guard<Spinlock> lock(random_lock);
    std::uniform_int_distribution<> dis(0, 1);
    r = dis(random_engine);
  }
  if (type == "direct")
    return new DirectMessenger(cct, name, lname, nonce, factory, new QueueStrategy(1));
  if (r == 0 || type == "simple")
    return new SimpleMessenger(cct, name, lname, nonce, factory);
  else if (r == 1 || type == "async")
    return new AsyncMessenger(cct, name, lname, nonce, factory);
#ifdef HAVE_XIO
  else if ((type == "xio") &&
	   cct->check_experimental_feature_enabled("ms-type-xio"))
    return new XioMessenger(cct, name, lname, nonce, cflags, factory);
#endif
  lderr(cct) << "unrecognized ms_type '" << type << "'" << dendl;
  return nullptr;
}

/*
 * Pre-calculate desired software CRC settings.  CRC computation may
 * be disabled by default for some transports (e.g., those with strong
 * hardware checksum support).
 */
int Messenger::get_default_crc_flags(md_config_t * conf)
{
  int r = 0;
  if (conf->ms_crc_data)
    r |= MSG_CRC_DATA;
  if (conf->ms_crc_header)
    r |= MSG_CRC_HEADER;
  return r;
}
