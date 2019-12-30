// <StaticInst> -*- C++ -*-

/**
 * \file   DecoderBase
 *
 * \brief  File that defines a static instruction
 */


#ifndef __DECODER_BASE_H__
#define __DECODER_BASE_H__

#include <inttypes.h>
#include <vector>

namespace sparta
{
    namespace decode
    {
        //! Opcode define
        typedef uint32_t Opcode;
        class DecoderBase
        {
        public:

            struct EMPair {
                uint32_t encoding;
                uint32_t mask;
            };

            const Opcode encoding;
            const Opcode mask;
            const std::vector<EMPair> exclude;
            const char * mnemonic;
            uint32_t instr_id;

            DecoderBase(Opcode en, Opcode m,
                        const std::vector<EMPair> & ex,
                        const char * mn, uint32_t id):
                encoding(en),mask(m),exclude(ex),mnemonic(mn),instr_id(id)
            {
            }

            // Needed for synthetic instructions
            DecoderBase(const char* mnemonic = "Synthetic"):
                DecoderBase(0, 0, {{0, 0}}, mnemonic, 0)
            {}

        };

    }
}

#endif
