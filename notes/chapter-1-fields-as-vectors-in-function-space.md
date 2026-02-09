# Chapter 1 — Fields as Vectors in Function Space (Functional Analysis Preliminaries)

We briefly revisit vector spaces, norms, and inner products only to fix the geometric language used later to interpret discretization as projection.

---

## 1.2 Vector spaces (revisited)

In finite-dimensional algebra, a vector is an element of a vector space $\mathbb{R}^n$ equipped with:

- addition
- scalar multiplication

A basis $\{e_i\}_{i=1}^n$ allows any vector $v$ to be written in coordinates as

$$
v = v_1 e_1 + v_2 e_2 + \cdots + v_n e_n.
$$

A set $V$ is called a **vector space** if it supports:

1. **Addition:** for any $u, v \in V$, $(u+v) \in V$
2. **Scalar multiplication:** for any scalar $\alpha \in \mathbb{R}$ and $u \in V$, $(\alpha u) \in V$
3. A notion of **length** (a norm)
4. A notion of **angle/alignment** (an inner product)

---

## 1.2.1 Norms

A **norm** is a map

$$
\|\cdot\| : V \to \mathbb{R}_{\ge 0}
$$

that assigns a non-negative “length” to vectors.

---

### 1.2.2 Norm axioms

A norm satisfies:

1. **Positivity:**

$$
\|v\|\ge 0,\quad \text{and }\ \|v\|=0 \iff v=0.
$$

2. **Homogeneity (scaling):**

$$
\|\alpha v\| = |\alpha|\,\|v\|.
$$

(Scaling changes magnitude, not direction.)

3. **Triangle inequality:**

$$
\|u+v\| \le \|u\|+\|v\|.
$$

(Shortest path property.)

---

### 1.2.3 Examples of norms (finite-dimensional)

For $x=(x_1,\dots,x_n)$:

- Euclidean norm:

$$
\|x\|_2 = \sqrt{x_1^2 + \cdots + x_n^2}.
$$

- 1-norm:

$$
\|x\|_1 = |x_1| + \cdots + |x_n|.
$$

- Infinity norm:

$$
\|x\|_\infty = \max_i |x_i|.
$$

---

## 1.2.4 Inner products

An **inner product** is a map

$$
\langle \cdot,\cdot\rangle : V \times V \to \mathbb{R}.
$$

---

### 1.2.5 Inner product axioms

For all $u,v,w \in V$ and scalars $\alpha$:

1. **Linearity (in the first argument):**

$$
\langle \alpha u + v, w\rangle = \alpha \langle u,w\rangle + \langle v,w\rangle.
$$

2. **Symmetry:**

$$
\langle u,v\rangle = \langle v,u\rangle.
$$

3. **Positive definiteness:**

$$
\langle v,v\rangle \ge 0,\quad \text{and }\ \langle v,v\rangle=0 \iff v=0.
$$

(A vector has positive “self-alignment”.)

---

### 1.2.6 Inner-product induced norm

The inner product induces a norm:

$$
\|v\| := \sqrt{\langle v,v\rangle}.
$$

This answers “how big is $v$?” in the geometry defined by $\langle\cdot,\cdot\rangle$.

---

## 1.3 Matrices as maps on vectors

A matrix represents a linear map between vector spaces:

$$
A:\mathbb{R}^n \to \mathbb{R}^m.
$$

Key rules for a linear map $M$:

- Scaling: $M(\alpha v)=\alpha M(v)$
- Addition: $M(u+v)=M(u)+M(v)$

If you know what the map does to basis vectors, you know what it does to every vector.

Example in $\mathbb{R}^2$:

If

$$
e_1=(1,0),\quad e_2=(0,1),
$$

and

$$
M(e_1)=(2,1),\quad M(e_2)=(-1,3),
$$

then for any $u=3e_1+5e_2$,

$$
M(u)=3M(e_1)+5M(e_2)=3(2,1)+5(-1,3).
$$

This is the meaning of “$Ax$”: applying a linear map defined by how it transforms the basis.

---

## 1.4 Function spaces as infinite-dimensional vector spaces

A finite-dimensional vector may be viewed as a list of components.

A **function** can be thought of as a vector with infinitely many degrees of freedom — an element of an infinite-dimensional vector space.

---

### 1.4.1 Basis functions and coordinates

Any vector can be written as a linear combination of basis vectors:

$$
v = \sum_i v_i e_i.
$$

Analogously, a function can be written as a linear combination of basis functions:

$$
f(x) = \sum_i c_i\,\phi_i(x),
$$

where $\{\phi_i\}$ are basis functions and $\{c_i\}$ are coefficients (coordinates).

---

### Operations carry over

If $f,g$ are functions on $\Omega$:

- Addition:
$$
(f+g)(x)=f(x)+g(x)
$$

- Scalar multiplication:
$$
(\alpha f)(x)=\alpha f(x)
$$

---

### Inner product and norm in function space

A standard choice is the $L^2(\Omega)$ inner product:

$$
\langle f,g\rangle = \int_{\Omega} f(x)\,g(x)\,dx,
$$

with corresponding norm

$$
\|f\|_{L^2} = \sqrt{\int_{\Omega} f(x)^2\,dx }.
$$

---

## 1.5 Operators as “matrices” on function spaces

In finite dimensions, $y=Ax$ expresses a linear map.

In function spaces, the analogous statement is

$$
f = \mathcal{L}u,
$$

where $\mathcal{L}$ is a (linear) operator acting on functions.

---

## 1.6 FEM as subspace approximation

The finite element method replaces an infinite-dimensional space $V$ with a finite-dimensional subspace $V_h\subset V$.

The approximate solution is written in a finite basis:

$$
u_h = \sum_{i=1}^N c_i\,\phi_i.
$$

Key interpretation:

- In FEM, the “solution” is not stored as a function; it is stored as a **vector of coefficients** in a function basis.
- Operators become **matrices** after projection into this finite representation.
- Discretization can be understood as projection of the true field onto a lower-dimensional subspace, exactly like projection in 3D vector algebra.

---

## 1.7 Summary (bridge to projections)

- A function is a vector in an infinite-dimensional vector space.
- A basis gives coordinates; norms and inner products give geometry.
- Operators generalize matrices.
- FEM is subspace approximation: represent the unknown in $V_h\subset V$ and project the governing operator accordingly.
