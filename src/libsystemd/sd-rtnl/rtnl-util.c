/*-*- Mode: C; c-basic-offset: 8; indent-tabs-mode: nil -*-*/

/***
 This file is part of systemd.

 Copyright (C) 2013 Tom Gundersen <teg@jklm.no>

  systemd is free software; you can redistribute it and/or modify it
  under the terms of the GNU Lesser General Public License as published by
  the Free Software Foundation; either version 2.1 of the License, or
  (at your option) any later version.

  systemd is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public License
  along with systemd; If not, see <http://www.gnu.org/licenses/>.
***/

#include <linux/rtnetlink.h>
#include <netinet/ether.h>

#include "sd-rtnl.h"

#include "rtnl-util.h"
#include "rtnl-internal.h"

int rtnl_set_link_name(sd_rtnl *rtnl, int ifindex, const char *name) {
        _cleanup_rtnl_message_unref_ sd_rtnl_message *message = NULL;
        int r;

        assert(rtnl);
        assert(ifindex > 0);
        assert(name);

        r = sd_rtnl_message_link_new(RTM_SETLINK, ifindex, &message);
        if (r < 0)
                return r;

        r = sd_rtnl_message_append_string(message, IFLA_IFNAME, name);
        if (r < 0)
                return r;

        r = sd_rtnl_call(rtnl, message, 0, NULL);
        if (r < 0)
                return r;

        return 0;
}

int rtnl_set_link_properties(sd_rtnl *rtnl, int ifindex, const char *alias,
                             const struct ether_addr *mac, unsigned mtu) {
        _cleanup_rtnl_message_unref_ sd_rtnl_message *message = NULL;
        bool need_update = false;
        int r;

        assert(rtnl);
        assert(ifindex > 0);

        if (!alias && !mac && mtu == 0)
                return 0;

        r = sd_rtnl_message_link_new(RTM_SETLINK, ifindex, &message);
        if (r < 0)
                return r;

        if (alias) {
                r = sd_rtnl_message_append_string(message, IFLA_IFALIAS, alias);
                if (r < 0)
                        return r;

                need_update = true;

        }

        if (mac) {
                r = sd_rtnl_message_append_ether_addr(message, IFLA_ADDRESS, mac);
                if (r < 0)
                        return r;

                need_update = true;
        }

        if (mtu > 0) {
                r = sd_rtnl_message_append_u32(message, IFLA_MTU, mtu);
                if (r < 0)
                        return r;

                need_update = true;
        }

        if  (need_update) {
                r = sd_rtnl_call(rtnl, message, 0, NULL);
                if (r < 0)
                        return r;
        }

        return 0;
}

int rtnl_message_new_synthetic_error(int error, uint32_t serial, sd_rtnl_message **ret) {
        struct nlmsgerr *err;
        int r;

        assert(error <= 0);

        r = message_new(ret, NLMSG_SPACE(sizeof(struct nlmsgerr)));
        if (r < 0)
                return r;

        (*ret)->hdr->nlmsg_len = NLMSG_LENGTH(sizeof(struct nlmsgerr));
        (*ret)->hdr->nlmsg_type = NLMSG_ERROR;
        (*ret)->hdr->nlmsg_seq = serial;

        err = NLMSG_DATA((*ret)->hdr);

        err->error = error;

        return 0;
}

bool rtnl_message_type_is_route(uint16_t type) {
        switch (type) {
                case RTM_NEWROUTE:
                case RTM_GETROUTE:
                case RTM_DELROUTE:
                        return true;
                default:
                        return false;
        }
}

bool rtnl_message_type_is_link(uint16_t type) {
        switch (type) {
                case RTM_NEWLINK:
                case RTM_SETLINK:
                case RTM_GETLINK:
                case RTM_DELLINK:
                        return true;
                default:
                        return false;
        }
}

bool rtnl_message_type_is_addr(uint16_t type) {
        switch (type) {
                case RTM_NEWADDR:
                case RTM_GETADDR:
                case RTM_DELADDR:
                        return true;
                default:
                        return false;
        }
}

int rtnl_message_link_get_ifname(sd_rtnl_message *message, const char **ret) {
        unsigned short type;
        void *name;

        assert(rtnl_message_type_is_link(message->hdr->nlmsg_type));

        while (sd_rtnl_message_read(message, &type, &name)) {
                if (type == IFLA_IFNAME) {
                        *ret = name;
                        return 0;
                }
        }

        return -ENOENT;
}
