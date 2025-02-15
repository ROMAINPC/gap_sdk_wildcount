# Copyright (C) 2020  GreenWaves Technologies, SAS

# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU Affero General Public License as
# published by the Free Software Foundation, either version 3 of the
# License, or (at your option) any later version.

# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU Affero General Public License for more details.

# You should have received a copy of the GNU Affero General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.

from collections import namedtuple

from .cnn_grouped_convolution_pool_relu import GroupedConvATParam
from .utils import at_bits

GEN_CONV_POOL_RELU = "CNN_ConvolutionPoolReLU"

# pylint: disable=too-many-arguments

ConvATParam = namedtuple('ConvATParam', [
    "ConvOper",
    "Fcx",
    "Fcy",
    "Dcx",
    "Dcy",
    "Scx",
    "Scy",
    "ConvPad"
])

NO_CONV = ConvATParam(ConvOper='KOP_NONE', Fcx=0, Fcy=0, Dcx=0, Dcy=0, Scx=0, Scy=0, ConvPad=0)


def is_dp(_):
    # if conv_q.calc_q == conv_q.acc_q and\
    #     conv_q.acc_q.bits > conv_q.out_qs[0].bits:
    #     cop = "KOP_CONV_DP"
    # else:
    #     cop = "KOP_CONV"
    return True


def gen_conv_at_params(params, conv_q, pad_compatibilities, do_dp=False):
    if params.is_depthwise_conv():
        assert params.multiplier == 1, "Multiplier not supported"
        assert not do_dp, "No DP output for DW convolution"
        cop = is_dp(conv_q) and "KOP_CONV_DWDP" or "KOP_CONV_DW"
    elif params.is_grouped_conv():
        cop = is_dp(conv_q) and "KOP_CONV_DP" or "KOP_CONV"
        return GroupedConvATParam(
            ConvOper=cop,
            GroupIn=params.groups,
            GroupOut=params.multiplier,
            Fcx=params.filter.w,
            Fcy=params.filter.h,
            Dcx=params.dilation.w,
            Dcy=params.dilation.h,
            Scx=params.stride.w,
            Scy=params.stride.h,
            ConvPad=params.has_at_zero_pad() and 1 or 0
        )
    else:
        cop = is_dp(conv_q) and "KOP_CONV_DP" or "KOP_CONV"

    pad_compatibilities.append(params.padding.pad_compatibility)
    return ConvATParam(
        ConvOper=cop,
        Fcx=params.filter.w,
        Fcy=params.filter.h,
        Dcx=params.dilation.w,
        Dcy=params.dilation.h,
        Scx=params.stride.w,
        Scy=params.stride.h,
        ConvPad=params.has_at_zero_pad() and 1 or 0
    )


PoolATParam = namedtuple('PoolATParam', [
    "PoolOper",
    "Fpx",
    "Fpy",
    "Dpx",
    "Dpy",
    "Spx",
    "Spy",
    "PoolPad"
])

NO_POOL = PoolATParam(PoolOper='KOP_NONE', Fpx=0, Fpy=0, Dpx=0, Dpy=0, Spx=0, Spy=0, PoolPad=0)


def gen_pool_at_params(params, pad_compatibilities):
    if params.pool_type == "average":
        pop = "KOP_AVGPOOL"
    elif params.pool_type == "max":
        pop = "KOP_MAXPOOL"
    else:
        raise NotImplementedError()

    pad_compatibilities.append(params.padding.pad_compatibility)
    return PoolATParam(
        PoolOper=pop,
        Fpx=params.filter.w,
        Fpy=params.filter.h,
        Dpx=1,
        Dpy=1,
        Spx=params.stride.w,
        Spy=params.stride.h,
        PoolPad=params.has_at_zero_pad() and 1 or 0
    )


ActivationATParam = namedtuple('ActivationATParam', [
    "ReLUOper"
])

NO_ACTIVATION = ActivationATParam(ReLUOper='KOP_NONE')


def gen_activation_op(activation):
    if activation is None or activation == "none":
        aop = "KOP_NONE"
    elif activation == "relu":
        aop = "KOP_RELU"
    elif activation == "relu6":
        aop = "KOP_RELUN"
    elif activation == "relun":
        aop = "KOP_RELUN"
    elif activation == "hsigmoid":
        aop = "KOP_HSIGMOID"
    elif activation == "swish" or activation == "hswish":
        aop = "KOP_HSWISH"
    elif activation == 'leaky':
        aop = "KOP_LEAKYRELU"
    elif activation == "sigmoid":
        aop = "KOP_SIGMOID"
    elif activation == "tanh":
        aop = "KOP_TANH"
    else:
        raise NotImplementedError("activation type %s not implemented" % activation)
    return aop


def gen_active_at_params(params):
    return ActivationATParam(
        ReLUOper=gen_activation_op(params.activation)
    )


