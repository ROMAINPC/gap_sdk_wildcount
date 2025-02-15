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

import numpy as np
from graph.types import ConstantInputParameters
from quantization.new_qrec import QRec
from utils.node_id import NodeId


class ConstantMixin():
    @classmethod
    def is_constant(cls, inp):
        return isinstance(inp[0], ConstantInputParameters)

    @classmethod
    def get_constant(cls, inp):
        params = inp[0]
        if not isinstance(params, ConstantInputParameters):
            raise ValueError("expected node %s to be constant input"%inp[0].name)
        return params.value

    @classmethod
    def optional_constant_scalar(cls, inputs, idx, default, dtype=np.float32):
        if len(inputs) <= idx:
            return dtype(default)
        val = cls.get_constant(inputs[idx]).flatten()[0]
        return val.astype(dtype)

    @classmethod
    def record_constant_qrec(cls, inp, cnode, **kwargs):
        qtype = inp[3]
        if qtype is None:
            return
        qrecs = kwargs.get('qrecs')
        if qrecs is None:
            return
        qrecs[NodeId(cnode)] = QRec.scaled(out_qs=[qtype])

    @classmethod
    def move_stat(cls, inp, new_name, **kwargs):
        cnid = NodeId(new_name)
        onid = NodeId(inp[0])
        qopts = kwargs.get('qopts', {})
        if onid in qopts:
            qopts[cnid] = qopts[onid]
            del qopts[onid]
