#pragma once

/** max_scale_with_room
 *  will return the max scaling factor for a number,
 *  leaving room for the outcome to be multiplied by 1e8
 */

uint128_t polcontract::max_scale_with_room(const uint128_t& num){
    if (num < SCALE_FACTOR_1E1) {
        return SCALE_FACTOR_1E26;
    } else if (num < SCALE_FACTOR_1E2) {
        return SCALE_FACTOR_1E25;
    } else if (num < SCALE_FACTOR_1E3) {
        return SCALE_FACTOR_1E24;
    } else if (num < SCALE_FACTOR_1E4) {
        return SCALE_FACTOR_1E23;
    } else if (num < SCALE_FACTOR_1E5) {
        return SCALE_FACTOR_1E22;
    } else if (num < SCALE_FACTOR_1E6) {
        return SCALE_FACTOR_1E21;
    } else if (num < SCALE_FACTOR_1E7) {
        return SCALE_FACTOR_1E20;
    } else if (num < SCALE_FACTOR_1E8) {
        return SCALE_FACTOR_1E19;
    } else if (num < SCALE_FACTOR_1E9) {
        return SCALE_FACTOR_1E18;
    } else if (num < SCALE_FACTOR_1E10) {
        return SCALE_FACTOR_1E17;
    } else if (num < SCALE_FACTOR_1E11) {
        return SCALE_FACTOR_1E16;
    } else if (num < SCALE_FACTOR_1E12) {
        return SCALE_FACTOR_1E15;
    } else if (num < SCALE_FACTOR_1E13) {
        return SCALE_FACTOR_1E14;
    } else if (num < SCALE_FACTOR_1E14) {
        return SCALE_FACTOR_1E13;
    } else if (num < SCALE_FACTOR_1E15) {
        return SCALE_FACTOR_1E12;
    } else if (num < SCALE_FACTOR_1E16) {
        return SCALE_FACTOR_1E11;
    } else if (num < SCALE_FACTOR_1E17) {
        return SCALE_FACTOR_1E10;
    } else if (num < SCALE_FACTOR_1E18) {
        return SCALE_FACTOR_1E9;
    } else if (num < SCALE_FACTOR_1E19) {
        return SCALE_FACTOR_1E8;
    } else if (num < SCALE_FACTOR_1E20) {
        return SCALE_FACTOR_1E7;
    } else if (num < SCALE_FACTOR_1E21) {
        return SCALE_FACTOR_1E6;
    } else if (num < SCALE_FACTOR_1E22) {
        return SCALE_FACTOR_1E5;
    } else if (num < SCALE_FACTOR_1E23) {
        return SCALE_FACTOR_1E4;
    } else if (num < SCALE_FACTOR_1E24) {
        return SCALE_FACTOR_1E3;
    } else if (num < SCALE_FACTOR_1E25) {
        return SCALE_FACTOR_1E2;
    } else if (num < SCALE_FACTOR_1E26) {
        return SCALE_FACTOR_1E1;
    } else {
        return 1;
    }
}


/** max_scale_without_room
 *  will return the max scaling factor for a number,
 *  leaving no room to multiply it any further
 */

uint128_t polcontract::max_scale_without_room(const uint128_t& num){
    if (num < SCALE_FACTOR_1E1) {
        return SCALE_FACTOR_1E34;
    } else if (num < SCALE_FACTOR_1E2) {
        return SCALE_FACTOR_1E33;
    } else if (num < SCALE_FACTOR_1E3) {
        return SCALE_FACTOR_1E32;
    } else if (num < SCALE_FACTOR_1E4) {
        return SCALE_FACTOR_1E31;
    } else if (num < SCALE_FACTOR_1E5) {
        return SCALE_FACTOR_1E30;
    } else if (num < SCALE_FACTOR_1E6) {
        return SCALE_FACTOR_1E29;
    } else if (num < SCALE_FACTOR_1E7) {
        return SCALE_FACTOR_1E28;
    } else if (num < SCALE_FACTOR_1E8) {
        return SCALE_FACTOR_1E27;
    } else if (num < SCALE_FACTOR_1E9) {
        return SCALE_FACTOR_1E26;
    } else if (num < SCALE_FACTOR_1E10) {
        return SCALE_FACTOR_1E25;
    } else if (num < SCALE_FACTOR_1E11) {
        return SCALE_FACTOR_1E24;
    } else if (num < SCALE_FACTOR_1E12) {
        return SCALE_FACTOR_1E23;
    } else if (num < SCALE_FACTOR_1E13) {
        return SCALE_FACTOR_1E22;
    } else if (num < SCALE_FACTOR_1E14) {
        return SCALE_FACTOR_1E21;
    } else if (num < SCALE_FACTOR_1E15) {
        return SCALE_FACTOR_1E20;
    } else if (num < SCALE_FACTOR_1E16) {
        return SCALE_FACTOR_1E19;
    } else if (num < SCALE_FACTOR_1E17) {
        return SCALE_FACTOR_1E18;
    } else if (num < SCALE_FACTOR_1E18) {
        return SCALE_FACTOR_1E17;
    } else if (num < SCALE_FACTOR_1E19) {
        return SCALE_FACTOR_1E16;
    } else if (num < SCALE_FACTOR_1E20) {
        return SCALE_FACTOR_1E15;
    } else if (num < SCALE_FACTOR_1E21) {
        return SCALE_FACTOR_1E14;
    } else if (num < SCALE_FACTOR_1E22) {
        return SCALE_FACTOR_1E13;
    } else if (num < SCALE_FACTOR_1E23) {
        return SCALE_FACTOR_1E12;
    } else if (num < SCALE_FACTOR_1E24) {
        return SCALE_FACTOR_1E11;
    } else if (num < SCALE_FACTOR_1E25) {
        return SCALE_FACTOR_1E10;
    } else if (num < SCALE_FACTOR_1E26) {
        return SCALE_FACTOR_1E9;
    } else if (num < SCALE_FACTOR_1E27) {
        return SCALE_FACTOR_1E8;
    } else if (num < SCALE_FACTOR_1E28) {
        return SCALE_FACTOR_1E7;
    } else if (num < SCALE_FACTOR_1E29) {
        return SCALE_FACTOR_1E6;
    } else if (num < SCALE_FACTOR_1E30) {
        return SCALE_FACTOR_1E5;
    } else if (num < SCALE_FACTOR_1E31) {
        return SCALE_FACTOR_1E4;
    } else if (num < SCALE_FACTOR_1E32) {
        return SCALE_FACTOR_1E3;
    } else if (num < SCALE_FACTOR_1E33) {
        return SCALE_FACTOR_1E2;
    } else {
        return 1;
    }
}