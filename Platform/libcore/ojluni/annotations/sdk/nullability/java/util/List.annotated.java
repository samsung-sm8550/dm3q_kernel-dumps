/*
 * Copyright (c) 1997, 2017, Oracle and/or its affiliates. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.  Oracle designates this
 * particular file as subject to the "Classpath" exception as provided
 * by Oracle in the LICENSE file that accompanied this code.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
 * or visit www.oracle.com if you need additional information or have any
 * questions.
 */


package java.util;


@SuppressWarnings({"unchecked", "deprecation", "all"})
public interface List<E> extends java.util.Collection<E> {

public int size();

public boolean isEmpty();

public boolean contains(@libcore.util.Nullable java.lang.Object o);

@libcore.util.NonNull public java.util.Iterator<@libcore.util.NullFromTypeParam E> iterator();

public java.lang.@libcore.util.Nullable Object @libcore.util.NonNull [] toArray();

// TODO: Make param and return types @Nullable T @NonNull [] once metalava supports TYPE_USE.
public <T> T @libcore.util.NonNull [] toArray(T @libcore.util.NonNull [] a);

public boolean add(@libcore.util.NullFromTypeParam E e);

public boolean remove(@libcore.util.Nullable java.lang.Object o);

public boolean containsAll(@libcore.util.NonNull java.util.Collection<?> c);

public boolean addAll(@libcore.util.NonNull java.util.Collection<? extends @libcore.util.NullFromTypeParam E> c);

public boolean addAll(int index, @libcore.util.NonNull java.util.Collection<? extends @libcore.util.NullFromTypeParam E> c);

public boolean removeAll(@libcore.util.NonNull java.util.Collection<?> c);

public boolean retainAll(@libcore.util.NonNull java.util.Collection<?> c);

public default void replaceAll(@libcore.util.NonNull java.util.function.UnaryOperator<@libcore.util.NullFromTypeParam E> operator) { throw new RuntimeException("Stub!"); }

public default void sort(@libcore.util.Nullable java.util.Comparator<? super @libcore.util.NullFromTypeParam E> c) { throw new RuntimeException("Stub!"); }

public void clear();

public boolean equals(@libcore.util.Nullable java.lang.Object o);

public int hashCode();

@libcore.util.NullFromTypeParam public E get(int index);

@libcore.util.NullFromTypeParam public E set(int index, @libcore.util.NullFromTypeParam E element);

public void add(int index, @libcore.util.NullFromTypeParam E element);

@libcore.util.NullFromTypeParam public E remove(int index);

public int indexOf(@libcore.util.Nullable java.lang.Object o);

public int lastIndexOf(@libcore.util.Nullable java.lang.Object o);

@libcore.util.NonNull public java.util.ListIterator<@libcore.util.NullFromTypeParam E> listIterator();

@libcore.util.NonNull public java.util.ListIterator<@libcore.util.NullFromTypeParam E> listIterator(int index);

@libcore.util.NonNull public java.util.List<@libcore.util.NullFromTypeParam E> subList(int fromIndex, int toIndex);

@libcore.util.NonNull public default java.util.Spliterator<@libcore.util.NullFromTypeParam E> spliterator() { throw new RuntimeException("Stub!"); }

@libcore.util.NonNull public static <E> java.util.List<@libcore.util.NonNull E> of() { throw new RuntimeException("Stub!"); }

@libcore.util.NonNull public static <E> java.util.List<@libcore.util.NonNull E> of(@libcore.util.NonNull E e1) { throw new RuntimeException("Stub!"); }

@libcore.util.NonNull public static <E> java.util.List<@libcore.util.NonNull E> of(@libcore.util.NonNull E e1, @libcore.util.NonNull E e2) { throw new RuntimeException("Stub!"); }

@libcore.util.NonNull public static <E> java.util.List<@libcore.util.NonNull E> of(@libcore.util.NonNull E e1, @libcore.util.NonNull E e2, @libcore.util.NonNull E e3) { throw new RuntimeException("Stub!"); }

@libcore.util.NonNull public static <E> java.util.List<@libcore.util.NonNull E> of(@libcore.util.NonNull E e1, @libcore.util.NonNull E e2, @libcore.util.NonNull E e3, @libcore.util.NonNull E e4) { throw new RuntimeException("Stub!"); }

@libcore.util.NonNull public static <E> java.util.List<@libcore.util.NonNull E> of(@libcore.util.NonNull E e1, @libcore.util.NonNull E e2, @libcore.util.NonNull E e3, @libcore.util.NonNull E e4, @libcore.util.NonNull E e5) { throw new RuntimeException("Stub!"); }

@libcore.util.NonNull public static <E> java.util.List<@libcore.util.NonNull E> of(@libcore.util.NonNull E e1, @libcore.util.NonNull E e2, @libcore.util.NonNull E e3, @libcore.util.NonNull E e4, @libcore.util.NonNull E e5, @libcore.util.NonNull E e6) { throw new RuntimeException("Stub!"); }

@libcore.util.NonNull public static <E> java.util.List<@libcore.util.NonNull E> of(@libcore.util.NonNull E e1, @libcore.util.NonNull E e2, @libcore.util.NonNull E e3, @libcore.util.NonNull E e4, @libcore.util.NonNull E e5, @libcore.util.NonNull E e6, @libcore.util.NonNull E e7) { throw new RuntimeException("Stub!"); }

@libcore.util.NonNull public static <E> java.util.List<@libcore.util.NonNull E> of(@libcore.util.NonNull E e1, @libcore.util.NonNull E e2, @libcore.util.NonNull E e3, @libcore.util.NonNull E e4, @libcore.util.NonNull E e5, @libcore.util.NonNull E e6, @libcore.util.NonNull E e7, @libcore.util.NonNull E e8) { throw new RuntimeException("Stub!"); }

@libcore.util.NonNull public static <E> java.util.List<@libcore.util.NonNull E> of(@libcore.util.NonNull E e1, @libcore.util.NonNull E e2, @libcore.util.NonNull E e3, @libcore.util.NonNull E e4, @libcore.util.NonNull E e5, @libcore.util.NonNull E e6, @libcore.util.NonNull E e7, @libcore.util.NonNull E e8, @libcore.util.NonNull E e9) { throw new RuntimeException("Stub!"); }

@libcore.util.NonNull public static <E> java.util.List<@libcore.util.NonNull E> of(@libcore.util.NonNull E e1, @libcore.util.NonNull E e2, @libcore.util.NonNull E e3, @libcore.util.NonNull E e4, @libcore.util.NonNull E e5, @libcore.util.NonNull E e6, @libcore.util.NonNull E e7, @libcore.util.NonNull E e8, @libcore.util.NonNull E e9, @libcore.util.NonNull E e10) { throw new RuntimeException("Stub!"); }

@java.lang.SafeVarargs
@libcore.util.NonNull public static <E> java.util.List<@libcore.util.NonNull E> of(E @libcore.util.NonNull ... elements) { throw new RuntimeException("Stub!"); }

@libcore.util.NonNull public static <E> java.util.List<@libcore.util.NonNull E> copyOf(@libcore.util.NonNull java.util.Collection<? extends E> coll) { throw new RuntimeException("Stub!"); }

public default void addFirst(@libcore.util.NullFromTypeParam E e) { throw new RuntimeException("Stub!"); }

public default void addLast(@libcore.util.NullFromTypeParam E e) { throw new RuntimeException("Stub!"); }

@libcore.util.NullFromTypeParam public default E getFirst() { throw new RuntimeException("Stub!"); }

@libcore.util.NullFromTypeParam public default E getLast() { throw new RuntimeException("Stub!"); }

@libcore.util.NullFromTypeParam public default E removeFirst() { throw new RuntimeException("Stub!"); }

@libcore.util.NullFromTypeParam public default E removeLast() { throw new RuntimeException("Stub!"); }

@libcore.util.NonNull public default java.util.List<E> reversed() { throw new RuntimeException("Stub!"); }

}