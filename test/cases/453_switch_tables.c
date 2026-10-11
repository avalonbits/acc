/* A switch finds its case by an index into a table of addresses where
 * the cases lie close together, on a char or an int; by searching a table
 * of values on an int whose cases are far apart; and by comparing each
 * otherwise. Values negative, past a short and at the char's ends; a case
 * the char cannot have, left out; values between the cases and either
 * side of them; no default, so none of them returns past the table;
 * fall-through; a switch inside a case of another; and one with three
 * hundred cases, whose search count needs both of its bytes. */

static int on_int(int x)
{
    switch (x) {
    case -100000: return 1;
    case -1: return 2;
    case 0: return 3;
    case 7: return 4;
    case 70000: return 5;
    case 8388607: return 6;
    }

    return 0;
}

static int two(int x)
{
    switch (x) {
    case 3: return 30;
    case 4: return 40;
    default: return -1;
    }
}

static int on_char(signed char c)
{
    int r = 0;

    switch (c) {
    case -128: r += 1;
    case 'a': r += 10; break;
    case 'z': r += 100; break;
    case 127: r += 1000; break;
    case 300: r += 5; break;   /* a value a char cannot have */
    }

    return r;
}

static int on_byte(unsigned char c)
{
    switch (c) {
    case 0: return 1;
    case 1: return 2;
    case 128: return 3;
    case 255: return 4;
    default: return 0;
    }
}

static int three(char c)
{
    switch (c) {
    case 'x': return 1;
    case 'y': return 2;
    case 'z': return 3;
    }

    return 0;
}

static int dense_char(signed char c)
{
    switch (c) {
    case -3: return 1;
    case -1: return 2;          /* -2 a hole */
    case 0: return 3;
    case 4: return 4;           /* 1 to 3 holes */
    }

    return 0;
}

static int dense_int(int x)
{
    switch (x) {
    case 1000: return 1;
    case 1002: return 2;
    case 1003: return 3;
    case 1005: return 4;
    case 1006: return 5;
    case 1009: return 6;
    }

    return -1;
}

static int nested(int a, char b)
{
    switch (a) {
    case 1:
        switch (b) {
        case 'p': return 11;
        case 'q': return 12;
        case 'r': return 13;
        case 's': return 14;
        }
        return 10;
    case 2:
        return 20;
    }

    return 0;
}

static int big(int x)
{
    switch (x) {
    case 0: return 1;
    case 3: return 2;
    case 6: return 3;
    case 9: return 4;
    case 12: return 5;
    case 15: return 6;
    case 18: return 7;
    case 21: return 8;
    case 24: return 9;
    case 27: return 10;
    case 30: return 11;
    case 33: return 12;
    case 36: return 13;
    case 39: return 14;
    case 42: return 15;
    case 45: return 16;
    case 48: return 17;
    case 51: return 18;
    case 54: return 19;
    case 57: return 20;
    case 60: return 21;
    case 63: return 22;
    case 66: return 23;
    case 69: return 24;
    case 72: return 25;
    case 75: return 26;
    case 78: return 27;
    case 81: return 28;
    case 84: return 29;
    case 87: return 30;
    case 90: return 31;
    case 93: return 32;
    case 96: return 33;
    case 99: return 34;
    case 102: return 35;
    case 105: return 36;
    case 108: return 37;
    case 111: return 38;
    case 114: return 39;
    case 117: return 40;
    case 120: return 41;
    case 123: return 42;
    case 126: return 43;
    case 129: return 44;
    case 132: return 45;
    case 135: return 46;
    case 138: return 47;
    case 141: return 48;
    case 144: return 49;
    case 147: return 50;
    case 150: return 51;
    case 153: return 52;
    case 156: return 53;
    case 159: return 54;
    case 162: return 55;
    case 165: return 56;
    case 168: return 57;
    case 171: return 58;
    case 174: return 59;
    case 177: return 60;
    case 180: return 61;
    case 183: return 62;
    case 186: return 63;
    case 189: return 64;
    case 192: return 65;
    case 195: return 66;
    case 198: return 67;
    case 201: return 68;
    case 204: return 69;
    case 207: return 70;
    case 210: return 71;
    case 213: return 72;
    case 216: return 73;
    case 219: return 74;
    case 222: return 75;
    case 225: return 76;
    case 228: return 77;
    case 231: return 78;
    case 234: return 79;
    case 237: return 80;
    case 240: return 81;
    case 243: return 82;
    case 246: return 83;
    case 249: return 84;
    case 252: return 85;
    case 255: return 86;
    case 258: return 87;
    case 261: return 88;
    case 264: return 89;
    case 267: return 90;
    case 270: return 91;
    case 273: return 92;
    case 276: return 93;
    case 279: return 94;
    case 282: return 95;
    case 285: return 96;
    case 288: return 97;
    case 291: return 98;
    case 294: return 99;
    case 297: return 100;
    case 300: return 101;
    case 303: return 102;
    case 306: return 103;
    case 309: return 104;
    case 312: return 105;
    case 315: return 106;
    case 318: return 107;
    case 321: return 108;
    case 324: return 109;
    case 327: return 110;
    case 330: return 111;
    case 333: return 112;
    case 336: return 113;
    case 339: return 114;
    case 342: return 115;
    case 345: return 116;
    case 348: return 117;
    case 351: return 118;
    case 354: return 119;
    case 357: return 120;
    case 360: return 121;
    case 363: return 122;
    case 366: return 123;
    case 369: return 124;
    case 372: return 125;
    case 375: return 126;
    case 378: return 127;
    case 381: return 128;
    case 384: return 129;
    case 387: return 130;
    case 390: return 131;
    case 393: return 132;
    case 396: return 133;
    case 399: return 134;
    case 402: return 135;
    case 405: return 136;
    case 408: return 137;
    case 411: return 138;
    case 414: return 139;
    case 417: return 140;
    case 420: return 141;
    case 423: return 142;
    case 426: return 143;
    case 429: return 144;
    case 432: return 145;
    case 435: return 146;
    case 438: return 147;
    case 441: return 148;
    case 444: return 149;
    case 447: return 150;
    case 450: return 151;
    case 453: return 152;
    case 456: return 153;
    case 459: return 154;
    case 462: return 155;
    case 465: return 156;
    case 468: return 157;
    case 471: return 158;
    case 474: return 159;
    case 477: return 160;
    case 480: return 161;
    case 483: return 162;
    case 486: return 163;
    case 489: return 164;
    case 492: return 165;
    case 495: return 166;
    case 498: return 167;
    case 501: return 168;
    case 504: return 169;
    case 507: return 170;
    case 510: return 171;
    case 513: return 172;
    case 516: return 173;
    case 519: return 174;
    case 522: return 175;
    case 525: return 176;
    case 528: return 177;
    case 531: return 178;
    case 534: return 179;
    case 537: return 180;
    case 540: return 181;
    case 543: return 182;
    case 546: return 183;
    case 549: return 184;
    case 552: return 185;
    case 555: return 186;
    case 558: return 187;
    case 561: return 188;
    case 564: return 189;
    case 567: return 190;
    case 570: return 191;
    case 573: return 192;
    case 576: return 193;
    case 579: return 194;
    case 582: return 195;
    case 585: return 196;
    case 588: return 197;
    case 591: return 198;
    case 594: return 199;
    case 597: return 200;
    case 600: return 201;
    case 603: return 202;
    case 606: return 203;
    case 609: return 204;
    case 612: return 205;
    case 615: return 206;
    case 618: return 207;
    case 621: return 208;
    case 624: return 209;
    case 627: return 210;
    case 630: return 211;
    case 633: return 212;
    case 636: return 213;
    case 639: return 214;
    case 642: return 215;
    case 645: return 216;
    case 648: return 217;
    case 651: return 218;
    case 654: return 219;
    case 657: return 220;
    case 660: return 221;
    case 663: return 222;
    case 666: return 223;
    case 669: return 224;
    case 672: return 225;
    case 675: return 226;
    case 678: return 227;
    case 681: return 228;
    case 684: return 229;
    case 687: return 230;
    case 690: return 231;
    case 693: return 232;
    case 696: return 233;
    case 699: return 234;
    case 702: return 235;
    case 705: return 236;
    case 708: return 237;
    case 711: return 238;
    case 714: return 239;
    case 717: return 240;
    case 720: return 241;
    case 723: return 242;
    case 726: return 243;
    case 729: return 244;
    case 732: return 245;
    case 735: return 246;
    case 738: return 247;
    case 741: return 248;
    case 744: return 249;
    case 747: return 250;
    case 750: return 251;
    case 753: return 252;
    case 756: return 253;
    case 759: return 254;
    case 762: return 255;
    case 765: return 256;
    case 768: return 257;
    case 771: return 258;
    case 774: return 259;
    case 777: return 260;
    case 780: return 261;
    case 783: return 262;
    case 786: return 263;
    case 789: return 264;
    case 792: return 265;
    case 795: return 266;
    case 798: return 267;
    case 801: return 268;
    case 804: return 269;
    case 807: return 270;
    case 810: return 271;
    case 813: return 272;
    case 816: return 273;
    case 819: return 274;
    case 822: return 275;
    case 825: return 276;
    case 828: return 277;
    case 831: return 278;
    case 834: return 279;
    case 837: return 280;
    case 840: return 281;
    case 843: return 282;
    case 846: return 283;
    case 849: return 284;
    case 852: return 285;
    case 855: return 286;
    case 858: return 287;
    case 861: return 288;
    case 864: return 289;
    case 867: return 290;
    case 870: return 291;
    case 873: return 292;
    case 876: return 293;
    case 879: return 294;
    case 882: return 295;
    case 885: return 296;
    case 888: return 297;
    case 891: return 298;
    case 894: return 299;
    case 897: return 300;
    }

    return -1;
}

int main(void)
{
    if (on_int(-100000) != 1 || on_int(-1) != 2 || on_int(0) != 3
        || on_int(7) != 4 || on_int(70000) != 5 || on_int(8388607) != 6
        || on_int(8) != 0)
        return 1;
    if (two(3) != 30 || two(4) != 40 || two(5) != -1)
        return 2;
    if (on_char(-128) != 11 || on_char('a') != 10 || on_char('z') != 100
        || on_char(127) != 1000 || on_char(44) != 0)
        return 3;
    if (on_byte(0) != 1 || on_byte(1) != 2 || on_byte(128) != 3
        || on_byte(255) != 4 || on_byte(7) != 0)
        return 4;
    if (three('y') != 2 || three('w') != 0)
        return 5;
    if (nested(1, 'q') != 12 || nested(1, 's') != 14 || nested(1, 't') != 10
        || nested(2, 'p') != 20 || nested(3, 'p') != 0)
        return 6;
    if (big(0) != 1 || big(897) != 300 || big(450) != 151 || big(1) != -1
        || big(900) != -1)
        return 7;
    if (dense_char(-3) != 1 || dense_char(-2) != 0 || dense_char(-1) != 2
        || dense_char(0) != 3 || dense_char(2) != 0 || dense_char(4) != 4
        || dense_char(-4) != 0 || dense_char(5) != 0 || dense_char(-128) != 0)
        return 8;
    if (dense_int(1000) != 1 || dense_int(1001) != -1 || dense_int(1003) != 3
        || dense_int(1009) != 6 || dense_int(999) != -1 || dense_int(1010) != -1
        || dense_int(-1000) != -1 || dense_int(1000 + 256) != -1)
        return 9;

    return 42;
}
