/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements. See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership. The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License. You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied. See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

package com.facebook.thrift;

import static org.hamcrest.Matchers.equalTo;
import static org.hamcrest.Matchers.not;
import static org.hamcrest.core.Is.is;
import static org.junit.Assert.assertThat;

import com.facebook.thrift.test.MySimpleStruct;
import junit.framework.TestCase;
import org.junit.Test;

public class StructTest extends TestCase {
  @Test
  public void testStructHashcode() throws Exception {
    MySimpleStruct defaultStruct = new MySimpleStruct();
    assertThat(defaultStruct.hashCode(), is(not(equalTo(0))));

    MySimpleStruct struct1 = new MySimpleStruct(1, "Foo");
    MySimpleStruct struct2 = new MySimpleStruct(2, "Bar");

    assertThat(struct1.hashCode(), is(not(equalTo(0))));
    assertThat(struct2.hashCode(), is(not(equalTo(0))));
    assertThat(struct1.hashCode(), is(not(equalTo(struct2.hashCode()))));
  }
}
