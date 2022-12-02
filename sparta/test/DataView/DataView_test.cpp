
#include <inttypes.h>
#include <iostream>

#include "sparta/sparta.hpp"
#include "sparta/simulation/TreeNode.hpp"
#include "sparta/functional/DataView.hpp"
#include "sparta/utils/SpartaTester.hpp"

/*!
 * \file DataView_test.cpp
 * \brief Test for DataView
 */

TEST_INIT

using sparta::ArchData;
using sparta::ArchDataSegment;
using sparta::DataView;

using sparta::LE; // Little
using sparta::BE; // Big


//! Prints an ArchData info. Also tests CONST correctness of query methods
void printArchData(const ArchData& a1k)
{
    if(a1k.isLaidOut()){
        std::cout << a1k.getSize() << " B used" << std::endl;
    }

    std::cout << std::dec
              << a1k.getLineSize() << " B sized lines" << std::endl
              << a1k.getNumAllocatedLines() << " Lines " << std::endl
              << a1k.isLaidOut() << " (layed out?)" << std::endl
              << a1k.getTotalWaste() << " B wasted (total)" << std::endl
              << a1k.getPaddingWaste() << " B wasted (padding)" << std::endl
              << a1k.getLineWaste() << " B wasted (line boundary)" << std::endl
              << "line states:" << std::endl
              << a1k.getLineStates() << std::endl;

    const ArchData::SegmentList& lst = a1k.getSegments();
    for(const ArchDataSegment* seg : lst){
        (void) seg; // Just iterate to test here. Too much clutter to print
    }
    std::cout << std::endl;
}

//! Print out list of segments and their info
void printSegList(const ArchData& a1k)
{
    const ArchData::SegmentList& lst = a1k.getSegments();
    for(const ArchDataSegment* seg : lst){
        std::cout << "  Segment : " << std::dec << seg->getLayoutID()
                  << " placed?:" << std::dec << seg->isPlaced()
                  << " @0x" << std::dec << seg->getOffset() << " size="
                  << std::dec << seg->getLayoutSize() << " B."<< std::endl;
        if(seg->getSubsetOf() != ArchDataSegment::INVALID_ID){
            std::cout << "    Is subset of id="
                      << std::hex << seg->getSubsetOf() << " +"
                      << seg->getSubsetOffset() << std::endl;
        }
        std::cout << "    arch="
                  << (const void*)seg->getArchData() << std::endl;
    }
    std::cout << std::endl;
}

int main()
{
    // Insantiation of ArchData

    ////////////////////////////////////////////////////////////////////////////
    //
    // WARNING
    //
    // We absolutely cannot layout a1k_other or a1 because their contents were
    // on the stack within EXPECT_* blocks. We just test population.
    //
    ////////////////////////////////////////////////////////////////////////////

    std::vector<DataView*> dvs;

    // Good ArchDatas
    ArchData a1(nullptr, 1);
    ArchData ainf(nullptr, 0); // Infinite line size
    ArchData bmax(nullptr, ArchData::MAX_LINE_SIZE); // Maximum line size
    ArchData a1k_other(nullptr, 1024); // 1k lines (used for temporary testing)
    ArchData ainf_other(nullptr, 0); // infinite lines (used for temporary testing)

    // Bad ArchDatas
    EXPECT_THROW(ArchData b(nullptr, 3)); // Non-power-of-2
    EXPECT_THROW(ArchData b(nullptr, 5)); // Non-power-of-2
    EXPECT_THROW(ArchData b(nullptr, 255)); // Non-power-of-2
    EXPECT_THROW(ArchData b(nullptr, 257)); // Non-power-of-2
    EXPECT_THROW(ArchData b(nullptr, ArchData::MAX_LINE_SIZE+1)); // Larger than max line size

    // Test extents of ArchData with 1-byte lines DataView appending
    EXPECT_NOTHROW(dvs.push_back(new DataView(&a1, 0, 1)));
    EXPECT_THROW(DataView x(&a1, 1, 2)); // Invalid
    EXPECT_THROW(DataView x(&a1, 2, 3)); // Invalid non-pow-2 and >1
    EXPECT_THROW(DataView x(&a1, 2, ArchData::MAX_LINE_SIZE)); // >1
    EXPECT_THROW(DataView x(&a1, 2, ArchData::MAX_LINE_SIZE+1)); // >1


    // Test some line-size extents

    DataView::ident_type oid = 100;


    // Good DataViews
    EXPECT_NOTHROW(dvs.push_back(new DataView(&a1k_other, 0, 8))); // MUST support views up to and including line size
    EXPECT_NOTHROW(dvs.push_back(new DataView(&a1k_other, 1, a1k_other.getLineSize()))); // MUST support views up to and including line size
    EXPECT_NOTHROW(dvs.push_back(new DataView(&ainf_other, oid++, ArchData::MAX_LINE_SIZE))); // MUST support MAX_LINE_SIZE

    // Illegal DataViews
    EXPECT_THROW(DataView x(&a1k_other, oid++, 0)); // Bad DataView size
    EXPECT_THROW(DataView x(&a1k_other, oid++, a1k_other.getLineSize()+1)); // Bad DataView size
    EXPECT_THROW(DataView x(&a1k_other, DataView::INVALID_ID, 0)); // Invalid id
    EXPECT_THROW(DataView x(&a1k_other, 0, 1)); // Duplicate ID (with dx0)


    // Test ArchData access alignment
    // Ensure that these reads and writes do not segfault
    EXPECT_NOTHROW(a1k_other.layout());
    EXPECT_TRUE(a1k_other.getSize() > 128); // Required for following read/write checks
    sparta::ArchData::Line& l = a1k_other.getLine(0); // for 128B access

    l.write<uint64_t, LE>(0, 0x8899aabbccddeeff, 0);
    l.write<uint64_t, LE>(0, 0x0011223344556677, 1);

    // Check a few values
    EXPECT_EQUAL((l.read<uint16_t, LE>(1)), 0xddee);
    EXPECT_EQUAL((l.read<uint32_t, LE>(1)), 0xbbccddee);
    EXPECT_EQUAL((l.read<uint64_t, LE>(2)), 0x66778899aabbccdd);
    EXPECT_EQUAL((l.read<uint64_t, LE>(3)), 0x5566778899aabbcc);


    std::cout << "Reading and writing misaligned lines: " << std::endl;

    std::cout << std::hex << "  ";
    for(int32_t x=1; x>=0; --x){
        std::cout << l.read<uint64_t, LE>(0, x);
    }
    std::cout << std::endl;

    for(uint32_t x=0; x<=2; ++x){
        std::cout << "  " << l.read<uint16_t, LE>(x, 0) << std::endl;
    }
    for(uint32_t x=0; x<=4; ++x){
        std::cout << "  " << l.read<uint32_t, LE>(x, 0) << std::endl;
    }
    for(uint32_t x=0; x<=sizeof(float); ++x){
        std::cout << "  " << l.read<float, LE>(x, 0) << std::endl;
    }
    for(uint32_t x=0; x<=8; ++x){
        std::cout << "  " << l.read<uint64_t, LE>(x, 0) << std::endl;
    }
    for(uint32_t x=0; x<=sizeof(double); ++x){
        std::cout << "  " << l.read<double, LE>(x, 0) << std::endl;
    }
    for(uint32_t x=0; x<=2; ++x){
        l.write<uint16_t, LE>(x, 1, 0);
    }
    for(uint32_t x=0; x<=4; ++x){
        l.write<uint32_t, LE>(x, 1, 0);
    }
    for(uint32_t x=0; x<=sizeof(float); ++x){
        l.write<float, LE>(x, 1, 0);
    }
    for(uint32_t x=0; x<=8; ++x){
        l.write<uint64_t, LE>(x, 1, 0);
    }
    for(uint32_t x=0; x<=sizeof(double); ++x){
        l.write<double, LE>(x, 1, 0);
    }
    std::cout << std::dec;

    //
    // Test ArchData + DataView with various reasonable non-zero block sizes
    //
    // In thes tests, DataViews are created on the stack and heap, but remain in
    // scope for layout.
    //

    const uint32_t block_sizes[] = {32, 64, 128, 0};//256, 512, 1024, 2048, 4096, 8192, 16384, 0};
    const uint32_t* pi = block_sizes;
    while(*pi != 0){
        uint32_t line_size = *pi;
        ++pi;

        ArchData a1k(nullptr, line_size); // 1k lines
        // Initial ArchData queries (no DataViews added)

        // Can layout with no lines, but prints warning
        //EXPECT_NOTHROW(a1k.layout()); // Cannot layout without any segments (DataViews) added


        // Setup some DataViews
        //
        // WARNING
        //
        // Test will fail if DataViews below are placed into EXPECT_* macros.
        // They must stay in scope until a1k.layout and dumping is complete.
        //

        // Various size independent DataViews
        DataView d1(&a1k, 0, 1, DataView::INVALID_ID, 0);

        // Check for double-registration failure
        EXPECT_THROW(a1k.registerSegment(&d1)); // Same exact segment already registered

        DataView d2(&a1k, 1, 2, DataView::INVALID_ID, 0);
        DataView d4(&a1k, 2, 4, DataView::INVALID_ID, 0);
        DataView d8(&a1k, 3, 8, DataView::INVALID_ID, 0);
        DataView d16(&a1k, 4, 16, DataView::INVALID_ID, 0);  // 128 b
        DataView d32(&a1k, 5, 32, DataView::INVALID_ID, 0);  // 256 b


        DataView do6(&a1k, 6, 8, DataView::INVALID_ID, 0);
        DataView do7(&a1k, 7, 16, DataView::INVALID_ID, 0);
        DataView do8(&a1k, 8, 16, DataView::INVALID_ID, 0);
        DataView do9(&a1k, 9, 16, DataView::INVALID_ID, 0);
        DataView do10(&a1k, 10, 8, DataView::INVALID_ID, 0);
        DataView do11(&a1k, 11, 8, DataView::INVALID_ID, 0);
        DataView do12(&a1k, 12, 16, DataView::INVALID_ID, 0);  // 128 b
        DataView do13(&a1k, 13, 16, DataView::INVALID_ID, 0);  // 128 b

        DataView::ident_type dvid = 1000;
        for(uint32_t i=0; i<64; ++i){
            dvs.push_back(new DataView(&a1k, dvid++, 4, DataView::INVALID_ID, 0));
        }

        for(uint32_t i=0; i<64; ++i){
            dvs.push_back(new DataView(&a1k, dvid++, 8, DataView::INVALID_ID, 0));
        }

        for(uint32_t i=0; i<64; ++i){
            dvs.push_back(new DataView(&a1k, dvid++, 16, DataView::INVALID_ID, 0));
        }

        for(uint32_t i=0; i<32; ++i){
            dvs.push_back(new DataView(&a1k, dvid++, 32, DataView::INVALID_ID, 0));
        }

        for(uint32_t i=0; i<32; ++i){
            DataView::ident_type parent_id = dvid++;
            dvs.push_back(new DataView(&a1k, parent_id, 32, DataView::INVALID_ID, 0));

            // Nest 4 views in each one of these
            for(uint32_t j=0; j<4; j++){
                DataView::ident_type parent_id2 = dvid++;
                dvs.push_back(new DataView(&a1k, parent_id2, 8, parent_id, j*8));

                // Nest 2 views in each of these
                for(uint32_t j=0; j<2; j++){
                    dvs.push_back(new DataView(&a1k, dvid++, 4, parent_id2, j*4));
                }
            }
        }

        // Some sparse, deep nesting, with out-of-order declaration.
        //
        // Layout/subsets      Sizes (B)
        //
        // |-------a-------|   a=16
        // |---b---/+++|-c-|   b=8  c=4
        //     |d|e|           d=2  e=2
        //       |f|           f=2
        //       g             g=1
        //
        // Important for test:
        //   Declare g before a - reverse nesting over multiple levels
        //   Declare e before d - reverse ordering by address
        //   Declare e before b - reverse nesting over 1 level
        //
        //   e same size as f - same-size subset
        //   multiple levels (more than 2)
        //   b, f, a do not have full subset - sparse subsets
        //   c, d, g have no subsets - empty subsets
        //   c begins multiple bytes after b ends - sparse subset in middle
        //   d begins at offset > 0 from b - sparse subset at start
        //   g begins at offset = 0 from f - sparse subset at end
        //
        DataView::ident_type id_a = dvid++;
        DataView::ident_type id_b = dvid++;
        DataView::ident_type id_c = dvid++;
        DataView::ident_type id_d = dvid++;
        DataView::ident_type id_e = dvid++;
        DataView::ident_type id_f = dvid++;
        DataView::ident_type id_g = dvid++;

        DataView* dv_c = new DataView(&a1k, id_c, 4,  id_a, 12);
        DataView* dv_g = new DataView(&a1k, id_g, 1,  id_f, 0);
        DataView* dv_e = new DataView(&a1k, id_e, 2,  id_b, 6);
        DataView* dv_b = new DataView(&a1k, id_b, 8,  id_a, 0);
        DataView* dv_d = new DataView(&a1k, id_d, 2,  id_b, 4);
        DataView* dv_a = new DataView(&a1k, id_a, 16);
        DataView* dv_f = new DataView(&a1k, id_f, 2,  id_e, 0);

        dvs.push_back(dv_a);
        dvs.push_back(dv_b);
        dvs.push_back(dv_c);
        dvs.push_back(dv_d);
        dvs.push_back(dv_e);
        dvs.push_back(dv_f);
        dvs.push_back(dv_g);

        // Pre-Layout DataView Validation

        EXPECT_THROW  (a1k.getSize()); // Not yet laid out
        EXPECT_TRUE   (a1k.getLineSize() == line_size);
        EXPECT_TRUE   (a1k.getNumAllocatedLines() == 0);
        EXPECT_TRUE   (a1k.getLineIndex(0)==0);
        EXPECT_TRUE   (a1k.getLineIndex(1)==0);
        EXPECT_TRUE   (a1k.getLineIndex(a1k.getLineSize()) - 1 == 0);
        EXPECT_TRUE   (a1k.getLineIndex(a1k.getLineSize()) == 1);
        EXPECT_THROW  (a1k.getLine(0)); // Not yet laid out
        EXPECT_TRUE   (a1k.getLineOffset(0) == 0);
        EXPECT_TRUE   (a1k.getLineOffset(1) == line_size);
        EXPECT_NOTHROW(a1k.checkCanAccess(0,0)); // Valid even without layout
        EXPECT_THROW  (a1k.checkDataSize(0)); // Invalid size
        EXPECT_NOTHROW(a1k.checkDataSize(a1k.getLineSize()));
        EXPECT_THROW  (a1k.checkDataSize(a1k.getLineSize()+1)); // Invalid size
        EXPECT_FALSE  (a1k.isLaidOut());
        EXPECT_TRUE   (a1k.getTotalWaste() == 0);
        EXPECT_TRUE   (a1k.getPaddingWaste() == 0);
        EXPECT_TRUE   (a1k.getLineWaste() == 0);
        EXPECT_TRUE   (a1k.getSegments().size() == 661);


        // Pre-layout ArchData inspection

        std::cout << "\nPre-layout" << std::endl;
        printArchData(a1k);


        // Perform Layout

        //EXPECT_NOTHROW(a1k.layout());
        a1k.layout();


        // Post-Layout Information

        EXPECT_TRUE(a1k.isLaidOut());
        EXPECT_THROW(a1k.layout()); // Cannot layout again

        std::cout << "\nPost-layout" << std::endl;
        printArchData(a1k);
        a1k.dumpLayout(std::cout);


        // Post-Layout DataView Validation

        // TODO: ...

        // Data I/O
        dv_a->write<uint64_t, LE>(0xdeadbeefdef3c8ed); // Writes to index 0
        dv_a->write<uint64_t, LE>(0xfeeda1fbadc0ffee, 1);

        std::cout << std::hex << dv_a->read<uint64_t, LE>()  << std::endl;
        std::cout << std::hex << dv_a->read<uint64_t, LE>(0) << std::endl;
        std::cout << std::hex << dv_a->read<uint64_t, LE>(1) << std::endl;
        std::cout << std::hex << dv_b->read<uint64_t, LE>(0) << std::endl;

        // Show the tree
        std::cout << "Tree content (LE reads): " << std::endl << std::hex;
        std::cout.fill('0');
        std::cout << "a: " << std::setw(8) << dv_a->read<uint64_t, LE>(1) << std::setw(8) << dv_a->read<uint64_t, LE>(0) << std::endl;
        std::cout << "b: " << std::setw(8) << dv_b->read<uint64_t, LE>() << "        " << std::setw(8) << dv_c->read<uint32_t, LE>() << std::endl;
        std::cout << "d:         " << std::setw(4) << dv_d->read<uint16_t, LE>() << std::setw(4) << dv_e->read<uint16_t, LE>() << std::endl;
        std::cout << "f:             " << std::setw(4) << dv_f->read<uint16_t, LE>() << std::endl;
        std::cout << "g:             " << std::setw(2) << (uint32_t)dv_g->read<uint8_t, LE>() << std::endl;

        std::cout << "Tree content (BE reads): " << std::endl << std::hex;
        std::cout.fill('0');
        std::cout << "a: " << std::setw(8) << dv_a->read<uint64_t, BE>(0) << std::setw(8) << dv_a->read<uint64_t, BE>(1) << std::endl;
        std::cout << "b: " << std::setw(8) << dv_b->read<uint64_t, BE>() << "        " << std::setw(8) << dv_c->read<uint32_t, BE>() << std::endl;
        std::cout << "d:         " << std::setw(4) << dv_d->read<uint16_t, BE>() << std::setw(4) << dv_e->read<uint16_t, BE>() << std::endl;
        std::cout << "f:             " << std::setw(4) << dv_f->read<uint16_t, BE>() << std::endl;
        std::cout << "g:             " << std::setw(2) << (uint32_t)dv_g->read<uint8_t, BE>() << std::endl;

        std::cout << "Tree content (by byte): " << std::endl;
        std::cout << "a:  " << dv_a->getByteString() << std::endl;
        std::cout << "b:  " << dv_b->getByteString() << "            " << dv_c->getByteString() << " :c" << std::endl;
        std::cout << "d:              " << dv_d->getByteString() << dv_e->getByteString() << " :e" << std::endl;
        std::cout << "f:                    " << dv_f->getByteString() << std::endl;
        std::cout << "g:                    " << dv_g->getByteString() << std::endl;


        // Check values
        // LE
        EXPECT_EQUAL( (dv_a->read<uint64_t, LE>() ), 0xdeadbeefdef3c8ed );
        EXPECT_EQUAL( (dv_a->read<uint64_t, LE>(0)), 0xdeadbeefdef3c8ed );
        EXPECT_EQUAL( (dv_a->read<uint64_t, LE>(1)), 0xfeeda1fbadc0ffee );
        EXPECT_EQUAL( (dv_a->read<uint64_t, LE>()),  (dv_b->read<uint64_t, LE>()) );
        EXPECT_EQUAL( (dv_a->read<uint32_t, LE>(3)), (dv_c->read<uint32_t, LE>()) );
        EXPECT_EQUAL( (dv_b->read<uint16_t, LE>(2)), (dv_d->read<uint16_t, LE>()) );
        EXPECT_EQUAL( (dv_b->read<uint16_t, LE>(3)), (dv_e->read<uint16_t, LE>()) );
        EXPECT_EQUAL( (dv_e->read<uint16_t, LE>()),  (dv_f->read<uint16_t, LE>()) );
        EXPECT_EQUAL( (dv_f->read<uint8_t,  LE>()),  (dv_g->read<uint8_t,  LE>()) );

        // BE
        EXPECT_EQUAL( (dv_a->read<uint64_t, BE>() ), 0xedc8f3deefbeadde );
        EXPECT_EQUAL( (dv_a->read<uint64_t, BE>(0)), 0xedc8f3deefbeadde );
        EXPECT_EQUAL( (dv_a->read<uint64_t, BE>(1)), 0xeeffc0adfba1edfe );
        EXPECT_EQUAL( (dv_a->read<uint64_t, BE>()),  (dv_b->read<uint64_t, BE>()) );
        EXPECT_EQUAL( (dv_a->read<uint32_t, BE>(3)), (dv_c->read<uint32_t, BE>()) );
        EXPECT_EQUAL( (dv_b->read<uint16_t, BE>(2)), (dv_d->read<uint16_t, BE>()) );
        EXPECT_EQUAL( (dv_b->read<uint16_t, BE>(3)), (dv_e->read<uint16_t, BE>()) );
        EXPECT_EQUAL( (dv_e->read<uint16_t, BE>()),  (dv_f->read<uint16_t, BE>()) );
        EXPECT_EQUAL( (dv_f->read<uint8_t,  BE>()),  (dv_g->read<uint8_t,  BE>()) );


        // Illegal reads
        EXPECT_THROW( (dv_b->read<uint64_t, LE>(1)) ); // b is 64b wide
        EXPECT_THROW( (dv_b->read<uint64_t, LE>(2)) ); // b is 64b wide
        EXPECT_THROW( (dv_c->read<uint64_t, LE>() ) ); // c is 32b wide
        EXPECT_THROW( (dv_c->read<uint32_t, LE>(1)) ); // c is 32b wide
        EXPECT_THROW( (dv_c->read<uint32_t, LE>(2)) ); // c is 32b wide

        // Clear ArchData internal data (resets)
        EXPECT_NOTEQUAL(a1k.getNumAllocatedLines(), 0);
        a1k.clean();
        EXPECT_EQUAL(a1k.getNumAllocatedLines(), 0);
    }

    // Free DataViews
    for(DataView* dv : dvs){
        delete dv;
    }

    //! \todo Test ArchData line invalidation

    // Done

    REPORT_ERROR;

    return ERROR_CODE;
}
