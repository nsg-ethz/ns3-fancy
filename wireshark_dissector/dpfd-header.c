/*!< Adding p4-mirror dissector to Wireshark */
/* dpfd-header.c
 * Edgar Costa Molero
 * Copyright 2019
 *
 * Wireshark - Network traffic analyzer
 * By Gerald Combs <gerald@wireshark.org>
 * Copyright 1998 Gerald Combs
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

/**
 *
 *       @file:       dpfd-header.c
 *       @brief:      Adding the dataplane fast detection header to wireshark
 *       @version:    0.1.1
 *       @pre:        wireshark version 2.2.5 and above
 *       @bug:        No Known bugs
 */

/*

Tutorial to create this: https://www.wireshark.org/docs/wsdg_html_chunked/ChDissectAdd.html
How to build for macos: https://wiki.wireshark.org/BuildingAndInstalling#macOS
*/ 

#include "config.h"
#include <epan/packet.h>
#include <stdio.h>

#define DPFD_ETHERTYPE 0x801

static int proto_dpfd = -1;
static gint ett_dpfd = -1;

static dissector_handle_t ip_handle = NULL;

// Action names
static const value_string actiontypes[] = {
    { 1, "GREY_START" },
    { 2, "GREY_STOP" },
    { 4, "GREY_COUNTER" },
    { 0, "KEEP_ALIVE" },
    { 20, "GREY_COUNTER|MULTIPLE_COUNTERS"},
    { 9, "GREY_START|COUNTER_MAXIMUMS"}
};

// Global variable fields

static int hf_dpfd_id = -1;
static int hf_dpfd_action = -1;

static int hf_dpfd_action_flag = -1;
static int hf_dpfd_action_ack = -1;
static int hf_dpfd_action_fsm = -1;
static int hf_dpfd_action_action = -1;

static int hf_dpfd_seq = -1;
static int hf_dpfd_data = -1;
static int hf_dpfd_next_header = -1;

static char * dpfd_flags_to_str(guint8 flag, guint8 ack, guint8 fsm)
{
  char *buf = "";
  if (!flag && !ack && !fsm)
  {
    return buf;
  }

  asprintf(&buf, "%s[", buf);

  if (flag)
  {
    asprintf(&buf, "%s%s", buf, "FLAG");
  }

  if (ack)
  {
    if (flag)
    {
      asprintf(&buf, "%s%s", buf, ", ");
    }
    asprintf(&buf, "%s%s", buf, "ACK");
  }

  if (fsm)
  {
    if (flag || ack)
    {
      asprintf(&buf, "%s%s", buf, ", ");
    }
    asprintf(&buf, "%s%s", buf, "FSM");
  }

  asprintf(&buf, "%s]", buf);

  return buf;
 
}

static int
dissect_dpfd(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree _U_, void *data _U_)
{
    col_set_str(pinfo->cinfo, COL_PROTOCOL, "DPFD");
    /* Clear out stuff in the info column */
    col_clear(pinfo->cinfo,COL_INFO);

    static const int *action_fields[] = {
      &hf_dpfd_action_flag,
      &hf_dpfd_action_ack,
      &hf_dpfd_action_fsm, 
      &hf_dpfd_action_action,
      NULL
    };

    guint32 id = tvb_get_guint32(tvb, 0, ENC_BIG_ENDIAN);
    guint8 action = tvb_get_guint8(tvb, 4);
    guint16 seq = tvb_get_guint16(tvb, 5, ENC_BIG_ENDIAN);
    guint64 counter = tvb_get_guint64(tvb, 7, ENC_BIG_ENDIAN);
    guint16 next = tvb_get_guint16(tvb, 15, ENC_BIG_ENDIAN);

    guint8 flag = (0x80 & action) >> 7;
    guint8 ack = (0x40 & action) >> 6;
    guint8 fsm = (0x20 & action) >> 5;

    action = 0x1f & action;

    col_add_fstr(pinfo->cinfo, COL_INFO, "id=%u %s Seq=%u Action=%s Counter=%llu", id,  dpfd_flags_to_str(flag, ack, fsm),seq,
             val_to_str(action, actiontypes, "Unknown (0x%02x)"), counter);

    proto_item *ti = proto_tree_add_item(tree, proto_dpfd, tvb, 0, 17, ENC_NA);

    proto_item_append_text(ti, ", id=%u %s Seq=%u Action=%s Counter=%llu", id, dpfd_flags_to_str(flag, ack, fsm),seq,
             val_to_str(action, actiontypes, "Unknown (0x%02x)"), counter);

    proto_tree *dpfd_tree = proto_item_add_subtree(ti, ett_dpfd);

    guint offset = 0;

    proto_tree_add_item(dpfd_tree, hf_dpfd_id, tvb, offset, 4, ENC_BIG_ENDIAN);
    offset +=4;

    proto_tree_add_bitmask(dpfd_tree, tvb, offset, hf_dpfd_action, ett_dpfd, action_fields, ENC_BIG_ENDIAN);
    offset +=1;

    proto_tree_add_item(dpfd_tree, hf_dpfd_seq, tvb, offset, 2, ENC_BIG_ENDIAN);
    offset +=2;

    proto_tree_add_item(dpfd_tree, hf_dpfd_data, tvb, offset, 8, ENC_BIG_ENDIAN);
    offset +=8;

    proto_tree_add_item(dpfd_tree, hf_dpfd_next_header, tvb, offset, 2, ENC_BIG_ENDIAN);
    offset +=2;


    if (next == 0x800)
    {
      return call_dissector(ip_handle, tvb_new_subset_remaining(tvb, offset), pinfo, tree);
    }

    return tvb_captured_length(tvb);
}

void
proto_register_dpfd(void)
{
    static hf_register_info hf[] = {
        {&hf_dpfd_id,
            { "DPFD id", "dpfd.id",
            FT_UINT32, BASE_DEC,
            NULL, 0x0,
            NULL, HFILL }
        },
        {&hf_dpfd_action,
            { "DPFD action", "dpfd.action",
            FT_UINT8, BASE_HEX,
            NULL, 0x0,
            NULL, HFILL }
        },
        {&hf_dpfd_action_flag,
            { "DPFD Action Counting Flag", "dpfd.action.flag",
            FT_BOOLEAN, 8,
            NULL, 0x80,
            NULL, HFILL}
        },
        {&hf_dpfd_action_ack,
            { "DPFD Action ACK Flag", "dpfd.action.ack",
            FT_BOOLEAN, 8,
            NULL, 0x40,
            NULL, HFILL }
        },
        {&hf_dpfd_action_fsm,
            { "DPFD Action FSM Flag", "dpfd.action.fsm",
            FT_BOOLEAN, 8,
            NULL, 0x20,
            NULL, HFILL }
        },
        {&hf_dpfd_action_action,
            { "DPFD Action Field", "dpfd.action.action",
            FT_UINT8, BASE_HEX,
            VALS(actiontypes), 0x1f,
            NULL, HFILL }
        },                                
        {&hf_dpfd_seq,
            { "DPFD seq number", "dpfd.seq",
            FT_UINT16, BASE_DEC,
            NULL, 0x0,
            NULL, HFILL }
        },
        {&hf_dpfd_data,
            { "DPFD data field", "dpfd.data",
            FT_UINT64, BASE_DEC,
            NULL, 0x0,
            NULL, HFILL }
        },
        {&hf_dpfd_next_header,
            { "DPFD next header", "dpfd.next",
            FT_UINT16, BASE_HEX,
            NULL, 0x0,
            NULL, HFILL }
        }
    };

    /* Setup protocol subtree array */
    static gint *ett[] = {
        &ett_dpfd
    };

    proto_dpfd = proto_register_protocol (
        "Dataplane Failure Detection Protocol", /* name       */
        "DPFD",      /* short name */
        "dpfd"       /* abbrev     */
    );

    proto_register_field_array(proto_dpfd, hf, array_length(hf));
    proto_register_subtree_array(ett, array_length(ett));

    register_dissector_table("dpfd.next", "DPFD next protocol", proto_dpfd, FT_UINT16, BASE_HEX);

}

void
proto_reg_handoff_dpfd(void)
{
    static dissector_handle_t dpfd_handle;

    dpfd_handle = create_dissector_handle(dissect_dpfd, proto_dpfd);
    dissector_add_uint("ethertype", DPFD_ETHERTYPE, dpfd_handle);

    ip_handle = find_dissector("ip");

}

