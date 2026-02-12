---
layout: default
title: Chapter 3 — Weak Forms as Operator Projections
parent: Table of Contents
nav_order: 2
---


# {{ page.title }}

In chapter 2 we introduced a powerful idea: approximation is projection. In chapter 3 we further develop this idea to apply to PDEs from a operator projection perspective. The key idea we will develop is that weak forms are operator projections.

---

## Contents
{: .no_toc .text-delta}
1. TOC
{:toc}

## 3.1: The PDE problem

We start with a (typical) PDE written as an operator equation:

$$
L u = f \qquad \text{in } \Omega
$$

where:

- $u$ is the unknown function (the “field”)
- $f$ is a given forcing / source
- $L$ is a differential operator (encodes the physics / constraints)
- $\Omega$ is the domain

In contrast to what was presented in chapter 2, this problem changes how we must view the projection:
> The object we must project is no longer a free vector - it is constrained by an operator equation.

---

## 3.2: Pointwise Enforcement - Why it fails

A natural first thought is to enforce the operator equation pointwise. This fails immediately because:

- Pointwise equality on a continuous operator imposes infinitely many constraints
- But $u_h$ belongs to a finite dimensional subspace
- A finite dimensional subspace cannot satisfy infinitely many independent constraints.

Hence, we cannot proceed. As such, we will look at enforcing the PDE through projection as we did earlier.

---

## 3.3: Projection of Continuous Operators

Define the **residual**:

$$
e := Lu - f
$$

Then the projection problem becomes

$$
\langle Lu - f,\; v \rangle = 0 \qquad \forall v \in V
$$

- the residual $e$ is **orthogonal** to the space of test functions $V$
- i.e. we are enforcing the PDE by making the error “invisible” under all test probes $v$

This is exactly the geometry of **projection**:

- Choose a subspace $V$
- Enforce orthogonality of the error/residual against $V$

We can rewrite the above equation in a more convenient form (from the linearity axiom of the inner product): 

$$
\langle Lu,\; v \rangle = \langle f,\; v \rangle \qquad \forall v \in V
$$


For this equation to make sense: 

- $L u_h$ must belong to the dual space of $v$
- $f$ must belong to the same space

That is, both sides of the equation must live in the same geometric space so that "equality of projections" is meaningful.

If $L$ contains derivatives, then those derivatives must exist in the chosen space, and the result must lie in the same space where the inner product is defined.

Let us consider a second order operator: 

$$
Lu = - \nabla^2 u
$$

If we now write $\langle L u_h \; v \rangle = \int_\Omega (- \nabla^2 u_h) v dx $, we are asking for second derivatives of $u_h$. If $u_h$ is not twice differentiable in $V$, we run into a problem because the projection will no longer be defined. 

We now integrate by parts the above equation to get: 

$$
\int_\Omega (\nabla^2 u_h) v dx \rightarrow \int_\Omega \nabla u_h \nabla v dx
$$

This step essentially passes the derivative from $u_h$ to $v$. Now both $u_h$ and $v$ must be once differentiable. Note that this is essentially the same problem expressed in a different form. We call this form as the "weak form" because it "weakens" the requirement of higher derivatives on the subspace.

---

## 3.4: The geometric meaning of integration by parts

Weak forms are not just “multiply and integrate”.
For differential operators, we typically **move derivatives** off $u$ onto $v$ using integration by parts (Green’s identity).

Integration by parts always produces

1. An **interior term** (domain integral)
2. A **boundary term** (integral over $\partial\Omega$)

For example, take the Poisson operator:

$$
Lu = -\Delta u
$$

Start from:

$$
\int_{\Omega} (-\Delta u)\, v \, dx = \int_{\Omega} f\, v\, dx
$$

Integrate by parts (Green’s identity):

$$
\int_{\Omega} \nabla u \cdot \nabla v \, dx \;-\; \int_{\partial\Omega} \frac{\partial u}{\partial n}\, v \, ds
= \int_{\Omega} f\, v\, dx
$$

- The first term is **interior geometry**
- The second term is **boundary geometry**

### 3.4 table: “what geometry is being measured?”

| After moving derivatives | Weak-form “measurement” | What it geometrically measures |
|---|---|---|
| No integration by parts | $\int_\Omega u\, v\, dx$ | **value / amplitude geometry** |
| One integration by parts | $\int_\Omega \nabla u \cdot \nabla v\, dx$ | **gradient geometry** |
| Two integrations by parts | $\int_\Omega (\nabla^2 u) (\nabla^2 v)\, dx$ | **curvature geometry** |

You can read this as: *moving derivatives changes which “feature” of the function the geometry cares about*. 

The first entry in the table is an expression that says "how close are the values of $u$ and $v$ in the space?"

The second entry in the table is an expression that says "how close are the derivatives of $u$ and $v$ in the space?" - the projection approximates variations, not values. 

The third entry in the table is an expression that says "how close are the curvatures of $u$ and $v$ in the space?" - the projection approximates curvatures, not values.

---

## 3.5: Operator order and function-space compatibility

Let $L$ be a differential operator of order $k$.

- A $k$-th order operator “wants” $k$ derivatives.
- But our FEM spaces only have certain smoothness / continuity.

**Core idea:**  
> The order of $L$ determines what geometry appears in the weak form, and that geometry dictates what function space is compatible.

### Examples

- $k=1$: geometry is about **values / fluxes** (easy)
- $k=2$: geometry becomes **gradients** (standard $H^1$ FEM)
- $k=3$: geometry would require **fractional curvature** (often incompatible / nonlocal)
- $k=4$: geometry is **curvature** (hard but possible; needs $H^2$)

### Key observation (even vs odd)

- **Even-order operators** can often be integrated by parts into a “symmetric” interior form (curvature / gradient squared). They can be mapped to Hilbert spaces.
- **Odd-order operators** tend to produce awkward boundary/nonlocal requirements and fractional smoothness. We need Sobolev fractional spaces to deal with such problems.

The next chapter will introduce the concepts of Hilbert spaces and Sobolev spaces more rigorously. 

---

## 3.6: Even order operators and geometric recoverability

For the moment we consider a fourth order operator $Lu = \Delta^2 u$
Its weak form contains the term $\int_\Omega \nabla^2 u : \nabla^2 v dx$

i.e., for fourth order derivatives the orthogonality is defined in terms of curvature. What subspace could we use for this projection?

**$H^1$ finite element spaces** are

- continuous in value ($C^0$)
- discontinuous in gradient across elements

So $H^1$ is usually not admissible.

**$H^2$ finite element spaces** require continuity of first derivatives:

- i.e. they need $C^1$ continuity (or an equivalent workaround)

So $H^2$ is **geometrically admissible** but **algebraically demanding**.

### Three strategies for geometric recoverability of even order operators

**1. $C^1$-conforming elements**

Examples:
- Hermite elements  
- Argyris triangle  
- Bogner–Fox–Schmit rectangle  

These enforce:
- function continuity
- gradient continuity

**2. Mixed formulations**

Rewrite a 4th order PDE as a coupled system of 2nd order PDEs.

Example:

$$
\Delta^2 u = f
\quad \Longrightarrow \quad
\begin{cases}
w = \Delta u \quad (1)\\
\Delta w = f \quad (2)
\end{cases}
$$

Now:

- each equation is second order
- standard $H^1$ FEM applies

**3. Weakly imposed continuity**

Examples:
- discontinuous Galerkin  
- interior penalty methods  
- $C^0$ interior penalty FEM  

Idea:

- allow discontinuous gradients
- penalize jumps weakly in the variational form

---

## 3.7: Geometric summary

- FEM is a **projection method**
- Projection requires a **well-defined inner product**
- Inner products define **geometry**
- Geometry dictates **function space compatibility**

### 3.7 table: operator order vs weak-form geometry vs FEM status

| Operator order | Weak-form geometry | FEM status |
|---:|---|---|
| 1 | value / flux | easy |
| 2 | gradient | standard |
| 3 | fractional curvature | incompatible |
| 4 | curvature | hard but possible |

---

## 3.8: The bilinear form

A form $a(u,v)$ is **bilinear** if:

1. It is linear in the first argument:
   $$
   a(u_1 + u_2,\; v) = a(u_1,\; v) + a(u_2,\; v)
   $$
2. It is linear in the second argument:
   $$
   a(u,\; v_1 + v_2) = a(u,\; v_1) + a(u,\; v_2)
   $$

All weak forms can be written as:

$$
a(u_h,\; v) = \ell(v)
$$

where:

- $a(\cdot,\cdot)$ is a bilinear form on $U \times V$
- $\ell(\cdot)$ is a linear functional on $V$

For instance, in the Poisson case:

$$
a(u, v) = \int_{\Omega} \nabla u \cdot \nabla v \, dx 
$$

$$
l(v) = \int_{\Omega} f\, v\, dx \;+\; \int_{\partial\Omega} \frac{\partial u}{\partial n}\, v \, ds
$$


### Concept map: finite vectors vs continuous functions

| Concept | Finite vectors | Functions / FEM |
|---|---|---|
| vector | $\mathbf{x} \in \mathbb{R}^n$ | $u(x) \in \mathbb{V}$ |
| dot product | $\mathbf{x}^T \mathbf{y}$ | $\int u\, v$ |
| generalized dot product | $\mathbf{x}^T A \mathbf{y}$ | $a(u,v)$ |
| matrix | $A$ | operator encoded by bilinear form |

---

## 3.9: From weak forms to linear systems

We start from the weak form:

$$
a(u_h,\; v) = \ell(v)
$$

Let:

$$
V_h = \text{span}\{\phi_1,\phi_2,\ldots,\phi_N\}
$$

and expand:

$$
u_h = \sum_{j=1}^{N} c_j \phi_j
$$

Since every $v$ is a linear combination of the basis functions $\phi_i$, we enforce the weak form on the basis tests:

$$
a(u_h,\; \phi_i) = \ell(\phi_i)
$$

Substitute $u_h$:

$$
a\!\left(\sum_{j=1}^{N} c_j \phi_j,\; \phi_i\right) = \ell(\phi_i)
$$

Use linearity:

$$
\sum_{j=1}^{N} c_j\, a(\phi_j,\phi_i) = \ell(\phi_i),
\qquad i=1,2,\ldots,N
$$

This is a system of $N$ linear equations in $N$ unknowns $c_j$.

Define two matrices/vectors:

1. Operator matrix:
   $$
   K_{ij} := a(\phi_j,\phi_i)
   $$
2. Load vector:
   $$
   b_i := \ell(\phi_i)
   $$

Then the discrete system is:

$$
\sum_{j=1}^{N} K_{ij} c_j = b_i
\quad\Longleftrightarrow\quad
Kc = b
$$

The operator matrix $K$ is also called the **stiffness matrix**.

---

## 3.10: Stiffness matrix as operator-induced geometry

Just as the mass matrix in Chapter 2 encoded the geometry of the subspace, the stiffness matrix encodes:

- geometry induced by the operator
- energy norms
- stability properties

**Key insight:**

> The mass matrix comes from expressing geometry (the projection problem) in coordinates (as we saw earlier).  
> The stiffness matrix comes from expressing the *operator induced geometry* of the solution space in coordinates.

---

## 3.11: Boundary conditions as part of geometry

In Chapter 2, everything was clean because projection lived entirely inside one space:

- we had space $V$
- we had an inner product
- we projected onto a subspace $V_h$

That worked because the inner product was closed: no “leaks”.

In PDEs, the moment we move derivatives around, the geometry “leaks” into the boundary.

To make sense of this statement, consider how integration by parts actually behaves. An integration by parts always produces: 

- An interior term (domain integral)
- A boundary term (integral over $\partial \Omega$)

Thus, the boundary term becomes a part of the solution, and by extension, a part of the operator induced geometry.

The boundary term has the generic form:

$$
\int_{\partial\Omega} (\text{flux}) \cdot (\text{test amplitude}) \, ds
$$

### Dirichlet boundary conditions: geometry by restriction

We enforce:

- $u = g$ on $\partial\Omega$
- $v = 0$ on $\partial\Omega$

So the boundary term vanishes, and the weak form becomes purely interior.

### Neumann boundary conditions: geometry by completion

We prescribe a flux:

$$
\frac{\partial u}{\partial n} = q \quad \text{on } \partial\Omega
$$

The boundary term becomes:

$$
-\int_{\partial\Omega} q\, v \, ds
$$

So the weak form becomes:

$$
\int_{\Omega} \nabla u \cdot \nabla v \, dx
=
\int_{\Omega} f\, v\, dx
+
\int_{\partial\Omega} q\, v \, ds
$$

**Key insight:**

> Dirichlet BCs constrain the trial space.  
> Neumann BCs contribute to the load functional.

Also note that boundary conditions are not “external loads” in general.  

> Some boundary conditions become loads after integration by parts; others fundamentally reshape the geometry of the space.

---

## 3.12: Function space geometry

Up to now we talked about functions as vectors.  
But vectors only become meaningful once we choose a geometry.

This section answers:

> “What does it mean for two functions to be close?”

### 3.12.1: Induced norms

Let $V$ be a vector space of functions. From an inner product we define the induced norm:

$$
\|u\|_V := \sqrt{\langle u,\; u\rangle}
$$

This norm measures the length of a function vector.

Once we have a norm, we automatically get a distance:

$$
d_V(u,v) := \|u-v\|_V
= \sqrt{\langle u-v,\; u-v\rangle}
$$

So the hierarchy is:

**inner product $\rightarrow$ norm $\rightarrow$ distance**

And geometry gives:

- length: $\|u\| = \sqrt{\langle u,u\rangle}$
- angle: $\cos\theta = \dfrac{\langle u,v\rangle}{\|u\|\|v\|}$
- distance: $d(u,v)=\sqrt{\langle u-v,u-v\rangle}$
- orthogonality: $\langle u,v\rangle = 0$

---

### 3.12.2: Amplitude geometry

On $V=L^2(\Omega)$ define:

$$
\langle u,\; v\rangle_{L^2}
:= \int_{\Omega} u(x)\, v(x)\, dx
$$

The induced norm is:

$$
\|u\|_{L^2} = \sqrt{\int_{\Omega} u(x)^2\, dx}
$$

What this geometry measures:

- similarity in amplitude
- alignment of function values
- total “energy”

---

### 3.12.3: Gradient geometry

Define:

$$
\langle u,\; v\rangle := \int_{\Omega} \nabla u \cdot \nabla v \, dx
$$

Then the induced norm is:

$$
\|u\| = \sqrt{\int_{\Omega} |\nabla u|^2\, dx}
$$

What this geometry measures:

- similarity in variation
- alignment of gradients
- deformation or strain energy

---

### 3.12.4: Curvature geometry (Second order geometry)

Functions that only differ by a constant are identical in this geometry.

Define:

$$
\langle u,\; v\rangle := \int_{\Omega} \nabla^2 u \cdot \nabla^2 v \, dx
$$

Induced norm:

$$
\|u\| = \sqrt{\int_{\Omega} (\nabla^2 u)^2\, dx}
$$

What this geometry measures:

- similarity in curvature
- bending energy
- higher order smoothness

---

### 3.12.5: Directional (Transport-weighted) geometry

Let $\beta(x)$ be a given vector field.

Define:

$$
\langle u,\; v\rangle := \int_{\Omega} (\beta\cdot\nabla u)\, (\beta\cdot\nabla v)\, dx
$$

Induced norm:

$$
\|u\|
= \sqrt{\int_{\Omega} (\beta\cdot\nabla u)^2\, dx}
$$

What this geometry measures:

- variation along flow direction
- transport-aligned similarity
- convection-dominated behaviour

---

### 3.12.6: Generalization — operator induced geometry

Let $B$ be a linear operator.

We measure a function by applying something to it and then taking an $L^2$ norm:

$$
\langle u,\; v\rangle_{B} := \int_{\Omega} (Bu)\,(Bv)\, dx
$$

So:

$$
\|u\|_{B} = \|Bu\|_{L^2}
= \sqrt{\int_{\Omega} (Bu)^2\, dx}
$$

This is called **operator induced geometry**.

#### 3.12.6 table: geometries as special cases of $B$

| Geometry | Operator |
|---|---|
| Amplitude geometry | $B = I$ |
| Variation geometry | $B = \nabla$ |
| Curvature geometry | $B = \nabla^2$ |
| Transport geometry | $B = \beta\cdot\nabla$ |

---

### 3.12.7: Implication for weak forms

Weak forms are not a mathematical trick. They are a geometric necessity.

Some operator-induced geometries:

- require derivatives that FEM spaces do not have
- cannot be evaluated pointwise

Integration by parts **changes the geometry**:

- from curvature-based
- to variation-based
