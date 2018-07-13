/* This file is part of the dynarmic project.
 * Copyright (c) 2018 MerryMage
 * This software may be used and distributed according to the terms of the GNU
 * General Public License version 2 or any later version.
 */

#include <boost/optional.hpp>
#include "common/bit_util.h"
#include "frontend/A64/translate/impl/impl.h"

namespace Dynarmic::A64 {
namespace {
enum class ComparisonType {
    EQ,
    GE,
    GT,
    HI,
    HS,
    LE,
    LT,
};

enum class ComparisonVariant {
    Register,
    Zero,
};

bool ScalarCompare(TranslatorVisitor& v, Imm<2> size, boost::optional<Vec> Vm, Vec Vn, Vec Vd,
                   ComparisonType type, ComparisonVariant variant) {
    if (size != 0b11) {
        return v.ReservedValue();
    }

    const size_t esize = 64;
    const size_t datasize = 64;

    const IR::U128 operand1 = v.V(datasize, Vn);
    const IR::U128 operand2 = variant == ComparisonVariant::Register ? v.V(datasize, Vm.get()) : v.ir.ZeroVector();

    const IR::U128 result = [&] {
        switch (type) {
        case ComparisonType::EQ:
            return v.ir.VectorEqual(esize, operand1, operand2);
        case ComparisonType::GE:
            return v.ir.VectorGreaterEqualSigned(esize, operand1, operand2);
        case ComparisonType::GT:
            return v.ir.VectorGreaterSigned(esize, operand1, operand2);
        case ComparisonType::HI:
            return v.ir.VectorGreaterUnsigned(esize, operand1, operand2);
        case ComparisonType::HS:
            return v.ir.VectorGreaterEqualUnsigned(esize, operand1, operand2);
        case ComparisonType::LE:
            return v.ir.VectorLessEqualSigned(esize, operand1, operand2);
        case ComparisonType::LT:
        default:
            return v.ir.VectorLessSigned(esize, operand1, operand2);
        }
    }();

    v.V_scalar(datasize, Vd, v.ir.VectorGetElement(esize, result, 0));
    return true;
}

enum class FPComparisonType {
    EQ,
    GE,
    AbsoluteGE,
    GT,
    AbsoluteGT
};

bool ScalarFPCompareRegister(TranslatorVisitor& v, bool sz, Vec Vm, Vec Vn, Vec Vd, FPComparisonType type) {
    const size_t esize = sz ? 64 : 32;
    const size_t datasize = esize;

    const IR::U128 operand1 = v.V(datasize, Vn);
    const IR::U128 operand2 = v.V(datasize, Vm);
    const IR::U128 result = [&] {
        switch (type) {
        case FPComparisonType::EQ:
            return v.ir.FPVectorEqual(esize, operand1, operand2);
        case FPComparisonType::GE:
            return v.ir.FPVectorGreaterEqual(esize, operand1, operand2);
        case FPComparisonType::AbsoluteGE:
            return v.ir.FPVectorGreaterEqual(esize,
                                             v.ir.FPVectorAbs(esize, operand1),
                                             v.ir.FPVectorAbs(esize, operand2));
        case FPComparisonType::GT:
            return v.ir.FPVectorGreater(esize, operand1, operand2);
        case FPComparisonType::AbsoluteGT:
            return v.ir.FPVectorGreater(esize,
                                        v.ir.FPVectorAbs(esize, operand1),
                                        v.ir.FPVectorAbs(esize, operand2));
        }

        UNREACHABLE();
        return IR::U128{};
    }();

    v.V_scalar(datasize, Vd, v.ir.VectorGetElement(esize, result, 0));
    return true;
}
} // Anonymous namespace

bool TranslatorVisitor::ADD_1(Imm<2> size, Vec Vm, Vec Vn, Vec Vd) {
    if (size != 0b11) {
        return ReservedValue();
    }
    const size_t esize = 8 << size.ZeroExtend<size_t>();
    const size_t datasize = esize;

    const IR::U64 operand1 = V_scalar(datasize, Vn);
    const IR::U64 operand2 = V_scalar(datasize, Vm);
    const IR::U64 result = ir.Add(operand1, operand2);
    V_scalar(datasize, Vd, result);
    return true;
}

bool TranslatorVisitor::CMEQ_reg_1(Imm<2> size, Vec Vm, Vec Vn, Vec Vd) {
    return ScalarCompare(*this, size, Vm, Vn, Vd, ComparisonType::EQ, ComparisonVariant::Register);
}

bool TranslatorVisitor::CMEQ_zero_1(Imm<2> size, Vec Vn, Vec Vd) {
    return ScalarCompare(*this, size, {}, Vn, Vd, ComparisonType::EQ, ComparisonVariant::Zero);
}

bool TranslatorVisitor::CMGE_reg_1(Imm<2> size, Vec Vm, Vec Vn, Vec Vd) {
    return ScalarCompare(*this, size, Vm, Vn, Vd, ComparisonType::GE, ComparisonVariant::Register);
}

bool TranslatorVisitor::CMGE_zero_1(Imm<2> size, Vec Vn, Vec Vd) {
    return ScalarCompare(*this, size, {}, Vn, Vd, ComparisonType::GE, ComparisonVariant::Zero);
}

bool TranslatorVisitor::CMGT_reg_1(Imm<2> size, Vec Vm, Vec Vn, Vec Vd) {
    return ScalarCompare(*this, size, Vm, Vn, Vd, ComparisonType::GT, ComparisonVariant::Register);
}

bool TranslatorVisitor::CMGT_zero_1(Imm<2> size, Vec Vn, Vec Vd) {
    return ScalarCompare(*this, size, {}, Vn, Vd, ComparisonType::GT, ComparisonVariant::Zero);
}

bool TranslatorVisitor::CMLE_1(Imm<2> size, Vec Vn, Vec Vd) {
    return ScalarCompare(*this, size, {}, Vn, Vd, ComparisonType::LE, ComparisonVariant::Zero);
}

bool TranslatorVisitor::CMLT_1(Imm<2> size, Vec Vn, Vec Vd) {
    return ScalarCompare(*this, size, {}, Vn, Vd, ComparisonType::LT, ComparisonVariant::Zero);
}

bool TranslatorVisitor::CMHI_1(Imm<2> size, Vec Vm, Vec Vn, Vec Vd) {
    return ScalarCompare(*this, size, Vm, Vn, Vd, ComparisonType::HI, ComparisonVariant::Register);
}

bool TranslatorVisitor::CMHS_1(Imm<2> size, Vec Vm, Vec Vn, Vec Vd) {
    return ScalarCompare(*this, size, Vm, Vn, Vd, ComparisonType::HS, ComparisonVariant::Register);
}

bool TranslatorVisitor::CMTST_1(Imm<2> size, Vec Vm, Vec Vn, Vec Vd) {
    if (size != 0b11) {
        return ReservedValue();
    }

    const IR::U128 operand1 = V(64, Vn);
    const IR::U128 operand2 = V(64, Vm);
    const IR::U128 anded = ir.VectorAnd(operand1, operand2);
    const IR::U128 result = ir.VectorNot(ir.VectorEqual(64, anded, ir.ZeroVector()));

    V(64, Vd, result);
    return true;
}

bool TranslatorVisitor::FABD_2(bool sz, Vec Vm, Vec Vn, Vec Vd) {
    const size_t esize = sz ? 64 : 32;

    const IR::U128 operand1 = V(esize, Vn);
    const IR::U128 operand2 = V(esize, Vm);
    const IR::U128 difference = ir.FPVectorAbsoluteDifference(esize, operand1, operand2);
    const IR::U128 result = ir.VectorZeroUpper(difference);

    V(128, Vd, result);
    return true;
}

bool TranslatorVisitor::FACGE_2(bool sz, Vec Vm, Vec Vn, Vec Vd) {
    return ScalarFPCompareRegister(*this, sz, Vm, Vn, Vd, FPComparisonType::AbsoluteGE);
}

bool TranslatorVisitor::FACGT_2(bool sz, Vec Vm, Vec Vn, Vec Vd) {
    return ScalarFPCompareRegister(*this, sz, Vm, Vn, Vd, FPComparisonType::AbsoluteGT);
}

bool TranslatorVisitor::FCMEQ_reg_2(bool sz, Vec Vm, Vec Vn, Vec Vd) {
    return ScalarFPCompareRegister(*this, sz, Vm, Vn, Vd, FPComparisonType::EQ);
}

bool TranslatorVisitor::FCMGE_reg_2(bool sz, Vec Vm, Vec Vn, Vec Vd) {
    return ScalarFPCompareRegister(*this, sz, Vm, Vn, Vd, FPComparisonType::GE);
}

bool TranslatorVisitor::FCMGT_reg_2(bool sz, Vec Vm, Vec Vn, Vec Vd) {
    return ScalarFPCompareRegister(*this, sz, Vm, Vn, Vd, FPComparisonType::GT);
}

bool TranslatorVisitor::SSHL_1(Imm<2> size, Vec Vm, Vec Vn, Vec Vd) {
    if (size != 0b11) {
        return ReservedValue();
    }

    const IR::U128 operand1 = V(64, Vn);
    const IR::U128 operand2 = V(64, Vm);
    const IR::U128 result = ir.VectorLogicalVShiftSigned(64, operand1, operand2);

    V(64, Vd, result);
    return true;
}

bool TranslatorVisitor::SUB_1(Imm<2> size, Vec Vm, Vec Vn, Vec Vd) {
    if (size != 0b11) {
        return ReservedValue();
    }
    const size_t esize = 8 << size.ZeroExtend<size_t>();
    const size_t datasize = esize;

    const IR::U64 operand1 = V_scalar(datasize, Vn);
    const IR::U64 operand2 = V_scalar(datasize, Vm);
    const IR::U64 result = ir.Sub(operand1, operand2);
    V_scalar(datasize, Vd, result);
    return true;
}

bool TranslatorVisitor::USHL_1(Imm<2> size, Vec Vm, Vec Vn, Vec Vd) {
    if (size != 0b11) {
        return ReservedValue();
    }

    const IR::U128 operand1 = V(64, Vn);
    const IR::U128 operand2 = V(64, Vm);
    const IR::U128 result = ir.VectorLogicalVShiftUnsigned(64, operand1, operand2);

    V(64, Vd, result);
    return true;
}

} // namespace Dynarmic::A64
