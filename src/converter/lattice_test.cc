// Copyright 2010, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include <string>
#include "base/base.h"
#include "converter/node.h"
#include "converter/lattice.h"
#include "testing/base/public/gunit.h"

namespace mozc {

TEST(LatticeTest, LatticeTest) {
  Lattice lattice;

  EXPECT_EQ("", lattice.key());
  EXPECT_FALSE(lattice.has_lattice());

  lattice.SetKey("this is a test");
  EXPECT_TRUE(lattice.has_lattice());

  EXPECT_TRUE(lattice.bos_nodes());
  EXPECT_TRUE(lattice.eos_nodes());

  lattice.Clear();
  EXPECT_EQ("", lattice.key());
  EXPECT_FALSE(lattice.has_lattice());
}

TEST(LatticeTest, NewNodeTest) {
  Lattice lattice;
  Node *node = lattice.NewNode();
  EXPECT_TRUE(node != NULL);
  EXPECT_EQ(0, node->lid);
  EXPECT_EQ(0, node->rid);
}

TEST(LatticeTest, InsertTest) {
  Lattice lattice;

  lattice.SetKey("test");

  {
    Node *node = lattice.NewNode();
    node->value = "ho";
    node->key = "es";
    lattice.Insert(1, node);

    Node *node2 = lattice.begin_nodes(1);
    EXPECT_EQ(node2, node);

    Node *node3 = lattice.end_nodes(3);
    EXPECT_EQ(node3, node);
  }

  {
    Node *node = lattice.NewNode();
    node->value = "o";
    node->key = "s";
    lattice.Insert(2, node);

    Node *node2 = lattice.begin_nodes(2);
    EXPECT_EQ(node2, node);

    int size = 0;
    Node *node3 = lattice.end_nodes(3);
    for (; node3 != NULL; node3 = node3->enext) {
      ++size;
    }
    EXPECT_EQ(2, size);
  }
}
}  // namespace mozc
