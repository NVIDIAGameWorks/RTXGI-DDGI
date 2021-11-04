# RTXGI Math Guide

## Irradiance

To find the illumination of a point on a surface in space, we compute the **irradiance** (<img src="https://render.githubusercontent.com/render/math?math=E">) at the point. <a href="http://www.pbr-book.org/3ed-2018/Color_and_Radiometry/Radiometry.html#x1-Radiance" target="_blank">Radiometry</a> defines irradiance as the flux of radiant energy per unit area _arriving_ at a surface. The complement of irradiance is **radiant exitance** (<img src="https://render.githubusercontent.com/render/math?math=M">), the flux of radiant energy per unit area _leaving_ a surface. Irradiance and radiant exitance; however, do not capture the directional distribution of energy. **Radiance** (<img src="https://render.githubusercontent.com/render/math?math=L_i">), the flux of radiant energy per unit projected area, per unit solid angle (e.g. along a single ray), provides the directional information needed to compute irradiance and radiant exitance.

To compute irradiance, we evaluate the **incoming radiance** (<img src="https://render.githubusercontent.com/render/math?math=L_i">) over the set of all directions (<img src="https://render.githubusercontent.com/render/math?math=\Omega">) in the hemisphere above the surface point. Mathematically, this is expressed by the integral of cosine-radiance over the hemisphere.

For a point <img src="https://render.githubusercontent.com/render/math?math=p"> with normal <img src="https://render.githubusercontent.com/render/math?math=\mathbf{n}">, irradiance <img src="https://render.githubusercontent.com/render/math?math=E"> is written as:
<figure>
<img src="images/math-eq-1.svg" width=300px></img>
<figcaption ><b>Equation 1: Irradiance</b></figcaption>
</figure>

where:
  * <img src="https://render.githubusercontent.com/render/math?math=L_i"> is the equation for incoming radiance at point <img src="https://render.githubusercontent.com/render/math?math=p"> from direction <img src="https://render.githubusercontent.com/render/math?math=\omega">.
  * <img src="https://render.githubusercontent.com/render/math?math=\theta"> is the angle between the incoming radiance direction <img src="https://render.githubusercontent.com/render/math?math=\omega"> and the surface normal <img src="https://render.githubusercontent.com/render/math?math=\mathbf{n}">.

Since the dot product represents the cosine of the angle between two vectors, Equation 1 can also be written as:
<figure>
<img src="images/math-eq-2.svg" width=350px></img>
<figcaption ><b>Equation 2: Irradiance reformulated with a dot product</b></figcaption>
</figure>

Note that the <img src="https://render.githubusercontent.com/render/math?math=cos\theta"> and <img src="https://render.githubusercontent.com/render/math?math=dot(\mathbf{n}, \omega)"> terms do not need to be clamped since we restrict the set of directions <img src="https://render.githubusercontent.com/render/math?math=\Omega"> to the hemisphere above the surface. This is illustrated in the figure below from <a href="http://www.pbr-book.org/3ed-2018/Color_and_Radiometry/Working_with_Radiometric_Integrals.html" target="_blank">Physically Based Rendering, 3rd Edition:</a>
<figure>
<img src="images/pbrt-irradiance-from-radiance.svg" width=400px></img>
<figcaption style="text-align:left; width:600px"><b>Figure 1: Irradiance at a point <i>p</i> is given by the integral of incoming radiance times the cosine of the incident direction over the entire upper hemisphere above the point.</b></figcaption>
</figure>

## Dynamic Diffuse Global Illumination (DDGI)

### Irradiance Integral Estimator

As is the case with all renderers, DDGI approximates <img src="https://render.githubusercontent.com/render/math?math=\E(p, n)"> since it can't be evaluated directly. We approximate the irradiance integral using a <a href="http://www.pbr-book.org/3ed-2018/Monte_Carlo_Integration/The_Monte_Carlo_Estimator.html" target="_blank">Monte Carlo Estimator</a> and a uniformly distributed set of sample directions.

The estimator transforms Equation 2 into:
<figure>
<img src="images/math-eq-ddgi-3.svg" style="width:550px"></img>
<figcaption style="text-align:left; width:600px"><b>Equation 3</b></figcaption>
</figure>

where <img src="https://render.githubusercontent.com/render/math?math=N"> is the number of **incoming radiance** directions (or samples). The summation represents the average value of the incoming radiance over the unit hemisphere multiplied by a constant term representing the integration domain (i.e. the area of the unit hemisphere). The approximate equality sign is used here to denote that the expected value of the quantity on the right is equal to the integral on the left, even though each individual average of <img src="https://render.githubusercontent.com/render/math?math=N"> samples will have some variance.

### Probe Storage and Lookup

In our implementation of DDGI, each probe irradiance texel stores the Monte Carlo Estimator defined in Equation 4 for a point <img src="https://render.githubusercontent.com/render/math?math=p"> and direction <img src="https://render.githubusercontent.com/render/math?math=\mathbf{n}"> defined by an octahedral parameterization of a sphere:

<figure>
<img src="images/math-eq-ddgi-4.svg" style="width:400px"></img>
<figcaption style="text-align:left; width:600px"><b>Equation 4</b></figcaption>
</figure>

To decrease variance in the estimate stored in the probes, we divide the sum of incoming radiance _by the sum of the cosine weights_ (instead of the number of radiance samples). This gives the quantity:
<figure>
<img src="images/math-eq-ddgi-5.svg" style="width:400px"></img>
<figcaption style="text-align:left; width:600px"><b>Equation 5</b></figcaption>
</figure>

But, we don't write this quantity exactly to the probe texels. To see why, consider that the <img src="https://render.githubusercontent.com/render/math?math=\sum_{i=1}^{N}dot(\mathbf{n}, \omega_i)"> term is a sum of <img src="https://render.githubusercontent.com/render/math?math=N"> cosine values uniformly distributed on the hemisphere. The expected value of this quantity is:

<figure>
<img src="images/math-eq-ddgi-6.svg" style="width:650px"></img>
<figcaption style="text-align:left; width:600px"><b>Equation 6</b></figcaption>
</figure>

Recalling Equation 4, we get an average by dividing by <img src="https://render.githubusercontent.com/render/math?math=N">. If we were to divide by <img src="https://render.githubusercontent.com/render/math?math=N/2"> above, the result would be off by a factor of 2. Therefore, we multiply the sum of the cosine weights by 2 before dividing by it to obtain the correct average. This final value is written to the probe texel:

<figure>
<img src="images/math-eq-ddgi-7.svg" style="width:1250px"></img>
</figure>

To estimate the irradiance integral, this quantity is read from probes and then multiplied by <img src="https://render.githubusercontent.com/render/math?math=2\pi">.

See `DDGIGetVolumeIrradiance()` in [Irradiance.hlsl](../rtxgi-sdk/shaders/ddgi/Irradiance.hlsl):

```c++
irradiance *= (2.f * RTXGI_PI); // Factored out of the probes
```

This yields the final estimate of the irradiance integral as described in Equation 3.
