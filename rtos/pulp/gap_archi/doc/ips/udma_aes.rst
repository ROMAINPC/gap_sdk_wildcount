.. 
   Input file: fe/ips/udma/udma_aes/README.md

Register map
^^^^^^^^^^^^


Overview
""""""""


Refer to :ref:`GAP9 address map<REF_MEMORY_MAP_DETAIL>` for the base address to be used.

.. table:: 
    :align: center
    :widths: 40 12 12 90

    +-------------------------------+------+-----+---------------------------------------+
    |             Name              |Offset|Width|              Description              |
    +===============================+======+=====+=======================================+
    |:ref:`KEY0_0<udma_aes__KEY0_0>`|     0|   32|Word 0 of encryption key               |
    +-------------------------------+------+-----+---------------------------------------+
    |:ref:`KEY0_1<udma_aes__KEY0_1>`|     4|   32|Word 1 of encryption key               |
    +-------------------------------+------+-----+---------------------------------------+
    |:ref:`KEY0_2<udma_aes__KEY0_2>`|     8|   32|Word 2 of encryption key               |
    +-------------------------------+------+-----+---------------------------------------+
    |:ref:`KEY0_3<udma_aes__KEY0_3>`|    12|   32|Word 3 of encryption key               |
    +-------------------------------+------+-----+---------------------------------------+
    |:ref:`KEY0_4<udma_aes__KEY0_4>`|    16|   32|Word 4 of encryption key               |
    +-------------------------------+------+-----+---------------------------------------+
    |:ref:`KEY0_5<udma_aes__KEY0_5>`|    20|   32|Word 5 of encryption key               |
    +-------------------------------+------+-----+---------------------------------------+
    |:ref:`KEY0_6<udma_aes__KEY0_6>`|    24|   32|Word 6 of encryption key               |
    +-------------------------------+------+-----+---------------------------------------+
    |:ref:`KEY0_7<udma_aes__KEY0_7>`|    28|   32|Word 7 of encryption key               |
    +-------------------------------+------+-----+---------------------------------------+
    |:ref:`IV0_0<udma_aes__IV0_0>`  |    32|   32|Word 0 of encrypted block initial value|
    +-------------------------------+------+-----+---------------------------------------+
    |:ref:`IV0_1<udma_aes__IV0_1>`  |    36|   32|Word 1 of encrypted block initial value|
    +-------------------------------+------+-----+---------------------------------------+
    |:ref:`IV0_2<udma_aes__IV0_2>`  |    40|   32|Word 2 of encrypted block initial value|
    +-------------------------------+------+-----+---------------------------------------+
    |:ref:`IV0_3<udma_aes__IV0_3>`  |    44|   32|Word 3 of encrypted block initial value|
    +-------------------------------+------+-----+---------------------------------------+
    |:ref:`DEST<udma_aes__DEST>`    |    48|   32|RX and TX destination channels         |
    +-------------------------------+------+-----+---------------------------------------+
    |:ref:`SETUP<udma_aes__SETUP>`  |    52|   32|Core setup                             |
    +-------------------------------+------+-----+---------------------------------------+
    |:ref:`CFG<udma_aes__CFG>`      |    56|   32|AES data flow configuration            |
    +-------------------------------+------+-----+---------------------------------------+

.. _udma_aes__KEY0_0:

KEY0_0
""""""

Word 0 of encryption key

.. table:: 
    :align: center
    :widths: 13 12 45 24 85

    +-----+---+--------+----------+-------------------------------------------------+
    |Bit #|R/W|  Name  |  Reset   |                   Description                   |
    +=====+===+========+==========+=================================================+
    |31:0 |W  |KEY_WORD|0x00000000|Value of the corresponding word of encryption key|
    +-----+---+--------+----------+-------------------------------------------------+

.. _udma_aes__KEY0_1:

KEY0_1
""""""

Word 1 of encryption key

.. table:: 
    :align: center
    :widths: 13 12 45 24 85

    +-----+---+--------+----------+-------------------------------------------------+
    |Bit #|R/W|  Name  |  Reset   |                   Description                   |
    +=====+===+========+==========+=================================================+
    |31:0 |W  |KEY_WORD|0x00000000|Value of the corresponding word of encryption key|
    +-----+---+--------+----------+-------------------------------------------------+

.. _udma_aes__KEY0_2:

KEY0_2
""""""

Word 2 of encryption key

.. table:: 
    :align: center
    :widths: 13 12 45 24 85

    +-----+---+--------+----------+-------------------------------------------------+
    |Bit #|R/W|  Name  |  Reset   |                   Description                   |
    +=====+===+========+==========+=================================================+
    |31:0 |W  |KEY_WORD|0x00000000|Value of the corresponding word of encryption key|
    +-----+---+--------+----------+-------------------------------------------------+

.. _udma_aes__KEY0_3:

KEY0_3
""""""

Word 3 of encryption key

.. table:: 
    :align: center
    :widths: 13 12 45 24 85

    +-----+---+--------+----------+-------------------------------------------------+
    |Bit #|R/W|  Name  |  Reset   |                   Description                   |
    +=====+===+========+==========+=================================================+
    |31:0 |W  |KEY_WORD|0x00000000|Value of the corresponding word of encryption key|
    +-----+---+--------+----------+-------------------------------------------------+

.. _udma_aes__KEY0_4:

KEY0_4
""""""

Word 4 of encryption key

.. table:: 
    :align: center
    :widths: 13 12 45 24 85

    +-----+---+--------+----------+-------------------------------------------------+
    |Bit #|R/W|  Name  |  Reset   |                   Description                   |
    +=====+===+========+==========+=================================================+
    |31:0 |W  |KEY_WORD|0x00000000|Value of the corresponding word of encryption key|
    +-----+---+--------+----------+-------------------------------------------------+

.. _udma_aes__KEY0_5:

KEY0_5
""""""

Word 5 of encryption key

.. table:: 
    :align: center
    :widths: 13 12 45 24 85

    +-----+---+--------+----------+-------------------------------------------------+
    |Bit #|R/W|  Name  |  Reset   |                   Description                   |
    +=====+===+========+==========+=================================================+
    |31:0 |W  |KEY_WORD|0x00000000|Value of the corresponding word of encryption key|
    +-----+---+--------+----------+-------------------------------------------------+

.. _udma_aes__KEY0_6:

KEY0_6
""""""

Word 6 of encryption key

.. table:: 
    :align: center
    :widths: 13 12 45 24 85

    +-----+---+--------+----------+-------------------------------------------------+
    |Bit #|R/W|  Name  |  Reset   |                   Description                   |
    +=====+===+========+==========+=================================================+
    |31:0 |W  |KEY_WORD|0x00000000|Value of the corresponding word of encryption key|
    +-----+---+--------+----------+-------------------------------------------------+

.. _udma_aes__KEY0_7:

KEY0_7
""""""

Word 7 of encryption key

.. table:: 
    :align: center
    :widths: 13 12 45 24 85

    +-----+---+--------+----------+-------------------------------------------------+
    |Bit #|R/W|  Name  |  Reset   |                   Description                   |
    +=====+===+========+==========+=================================================+
    |31:0 |W  |KEY_WORD|0x00000000|Value of the corresponding word of encryption key|
    +-----+---+--------+----------+-------------------------------------------------+

.. _udma_aes__IV0_0:

IV0_0
"""""

Word 0 of encrypted block initial value

.. table:: 
    :align: center
    :widths: 13 12 45 24 85

    +-----+---+----------+----------+--------------------------------------------------------------+
    |Bit #|R/W|   Name   |  Reset   |                         Description                          |
    +=====+===+==========+==========+==============================================================+
    |31:0 |W  |BLOCK_WORD|0x00000000|Value of the corresponding word of the initial encrypted block|
    +-----+---+----------+----------+--------------------------------------------------------------+

.. _udma_aes__IV0_1:

IV0_1
"""""

Word 1 of encrypted block initial value

.. table:: 
    :align: center
    :widths: 13 12 45 24 85

    +-----+---+----------+----------+--------------------------------------------------------------+
    |Bit #|R/W|   Name   |  Reset   |                         Description                          |
    +=====+===+==========+==========+==============================================================+
    |31:0 |W  |BLOCK_WORD|0x00000000|Value of the corresponding word of the initial encrypted block|
    +-----+---+----------+----------+--------------------------------------------------------------+

.. _udma_aes__IV0_2:

IV0_2
"""""

Word 2 of encrypted block initial value

.. table:: 
    :align: center
    :widths: 13 12 45 24 85

    +-----+---+----------+----------+--------------------------------------------------------------+
    |Bit #|R/W|   Name   |  Reset   |                         Description                          |
    +=====+===+==========+==========+==============================================================+
    |31:0 |W  |BLOCK_WORD|0x00000000|Value of the corresponding word of the initial encrypted block|
    +-----+---+----------+----------+--------------------------------------------------------------+

.. _udma_aes__IV0_3:

IV0_3
"""""

Word 3 of encrypted block initial value

.. table:: 
    :align: center
    :widths: 13 12 45 24 85

    +-----+---+----------+----------+--------------------------------------------------------------+
    |Bit #|R/W|   Name   |  Reset   |                         Description                          |
    +=====+===+==========+==========+==============================================================+
    |31:0 |W  |BLOCK_WORD|0x00000000|Value of the corresponding word of the initial encrypted block|
    +-----+---+----------+----------+--------------------------------------------------------------+

.. _udma_aes__DEST:

DEST
""""

RX and TX destination channels

.. table:: 
    :align: center
    :widths: 13 12 45 24 85

    +-----+---+-------+-----+---------------------------------------------------------------------+
    |Bit #|R/W| Name  |Reset|                             Description                             |
    +=====+===+=======+=====+=====================================================================+
    |7:0  |R/W|RX_DEST|0xFF |Stream ID for the RX uDMA channel. Default is 0xFF (channel disabled)|
    +-----+---+-------+-----+---------------------------------------------------------------------+
    |15:8 |R/W|TX_DEST|0xFF |Stream ID for the TX uDMA channel. Default is 0xFF (channel disabled)|
    +-----+---+-------+-----+---------------------------------------------------------------------+

.. _udma_aes__SETUP:

SETUP
"""""

Core setup

.. table:: 
    :align: center
    :widths: 13 12 45 24 85

    +-----+---+---------+-----+--------------------------------------------------+
    |Bit #|R/W|  Name   |Reset|                   Description                    |
    +=====+===+=========+=====+==================================================+
    |    0|R  |KEY_INIT |0x0  |Is set to 1 when the key configuration is finished|
    +-----+---+---------+-----+--------------------------------------------------+
    |    1|R/W|KEY_TYPE |0x0  |Key type: b0: 128 bits; b1: 256 bits              |
    +-----+---+---------+-----+--------------------------------------------------+
    |    2|R/W|ENC_DEC  |0x0  |Operation type: b0: decryption; b1: encryption    |
    +-----+---+---------+-----+--------------------------------------------------+
    |    3|R/W|ECB_CBC  |0x0  |Encryption type: b0: ECB; b1: CBC                 |
    +-----+---+---------+-----+--------------------------------------------------+
    |    4|W  |BLOCK_RST|0x0  |Write b1 to reset AES core                        |
    +-----+---+---------+-----+--------------------------------------------------+
    |    5|R/W|QK_KEY_EN|0x0  |Use quiddikey key generation                      |
    +-----+---+---------+-----+--------------------------------------------------+
    |    8|W  |FIFO_CLR |0x0  |Write b1 to clear data FIFO                       |
    +-----+---+---------+-----+--------------------------------------------------+

.. _udma_aes__CFG:

CFG
"""

AES data flow configuration

.. table:: 
    :align: center
    :widths: 13 12 45 24 85

    +-----+---+----+-----+---------------------------------------------------------------------------------------------------------+
    |Bit #|R/W|Name|Reset|                                               Description                                               |
    +=====+===+====+=====+=========================================================================================================+
    |1:0  |R/W|MODE|0x0  |Transfer mode: b00: memory to memory; b01: stream to memory; b10: memory to stream; b11: stream to stream|
    +-----+---+----+-----+---------------------------------------------------------------------------------------------------------+
