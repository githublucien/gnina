/*

 Copyright (c) 2006-2010, The Scripps Research Institute

 Licensed under the Apache License, Version 2.0 (the "License");
 you may not use this file except in compliance with the License.
 You may obtain a copy of the License at

 http://www.apache.org/licenses/LICENSE-2.0

 Unless required by applicable law or agreed to in writing, software
 distributed under the License is distributed on an "AS IS" BASIS,
 WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 See the License for the specific language governing permissions and
 limitations under the License.

 Author: Dr. Oleg Trott <ot14@columbia.edu>,
 The Olson Lab,
 The Scripps Research Institute

 */
#include <string>
#include "grid.h"
#include "grid_dim.h"
#include "common.h"

//evaluate using grid, if deriv is null, do not calc deriviative
fl grid::evaluate(const atom& a, const vec& location, fl slope, fl c,
		vec *deriv /*=NULL*/) const
		{
	//charge indep
	fl ret = evaluate_aux(data, location, slope, c, deriv);
	if (a.charge != 0 && chargedata.dim0() > 0)
	{
		//charge dependent
		if(deriv == NULL)
		{
			ret += a.charge * evaluate_aux(chargedata, location, slope, c, NULL);
		}
		else //otherwise, must add derivatives
		{
			vec cderiv(0,0,0);
			ret += a.charge * evaluate_aux(chargedata, location, slope, c, &cderiv);
			*deriv += a.charge*cderiv;
		}
	}
	return ret;
}

fl grid::evaluate_user(const vec& location, vec *deriv)
{
    return evaluate_aux(data, location, (fl) 1.0, (fl) 1.0, deriv);
}

//allocate memory for grid (but don't fill in values)
//only initialize charge dependent values if hashcharged is true
void grid::init(const grid_dims& gd, bool hascharged)
{
	data.resize(gd[0].n + 1, gd[1].n + 1, gd[2].n + 1);
	if(hascharged)
		chargedata.resize(gd[0].n + 1, gd[1].n + 1, gd[2].n + 1);
	m_init = vec(gd[0].begin, gd[1].begin, gd[2].begin);
	m_range = vec(gd[0].span(), gd[1].span(), gd[2].span());
	assert(m_range[0] > 0);
	assert(m_range[1] > 0);
	assert(m_range[2] > 0);
	m_dim_fl_minus_1 = vec(data.dim0() - 1.0,
			data.dim1() - 1.0,
			data.dim2() - 1.0);
	VINA_FOR(i, 3)
	{
		m_factor[i] = m_dim_fl_minus_1[i] / m_range[i];
		m_factor_inv[i] = 1 / m_factor[i];
	}
}

void grid::init(const grid_dims& gd, std::istream& user_in)
{
    data.resize(gd[0].n+1, gd[1].n+1, gd[2].n+1);
    m_init = vec(gd[0].begin, gd[1].begin, gd[2].begin);
	m_range = vec(gd[0].span(), gd[1].span(), gd[2].span());
	assert(m_range[0] > 0);
	assert(m_range[1] > 0);
	assert(m_range[2] > 0);
	m_dim_fl_minus_1 = vec(data.dim0()-1,
			data.dim1()-1,
			data.dim2()-1);
    
    std::string line;
    std::cout << "I'm not in the loop" << std::endl;
    std::cout << gd[2].n << std::endl;
    print(gd);
    VINA_FOR(z, gd[2].n+1)
    {
        VINA_FOR(y, gd[1].n+1)
        {
            VINA_FOR(x, gd[0].n+1)
            {
                std::getline(user_in,line);
                data(x, y, z) = ::atof(line.c_str());                
            }
        }        
    } 
    std::cout << data(4,0,0) << "\n";
}


fl grid::evaluate_aux(const array3d<fl>& m_data, const vec& location, fl slope,
		fl v, vec* deriv) const
		{ // sets *deriv if not NULL
	vec s = elementwise_product(location - m_init, m_factor);

	vec miss(0, 0, 0);
	boost::array<int, 3> region;
	boost::array<sz, 3> a;

	VINA_FOR(i, 3)
	{
		if (s[i] < 0)
		{
			miss[i] = -s[i];
			region[i] = -1;
			a[i] = 0;
			s[i] = 0;
		}
		else if (s[i] >= m_dim_fl_minus_1[i])
		{
			miss[i] = s[i] - m_dim_fl_minus_1[i];
			region[i] = 1;
			assert(m_data.dim(i) >= 2);
			a[i] = m_data.dim(i) - 2;
			s[i] = 1;
		}
		else
		{
			region[i] = 0; // now that region is boost::array, it's not initialized
			a[i] = sz(s[i]);
			s[i] -= a[i];
		}
		assert(s[i] >= 0);
		assert(s[i] <= 1);
		assert(a[i] >= 0);
		assert(a[i]+1 < m_data.dim(i));
	}
	const fl penalty = slope * (miss * m_factor_inv); // FIXME check that inv_factor is correctly initialized and serialized
	assert(penalty > -epsilon_fl);

	const sz x0 = a[0];
	const sz y0 = a[1];
	const sz z0 = a[2];

	const sz x1 = x0 + 1;
	const sz y1 = y0 + 1;
	const sz z1 = z0 + 1;

	const fl f000 = m_data(x0, y0, z0);
	const fl f100 = m_data(x1, y0, z0);
	const fl f010 = m_data(x0, y1, z0);
	const fl f110 = m_data(x1, y1, z0);
	const fl f001 = m_data(x0, y0, z1);
	const fl f101 = m_data(x1, y0, z1);
	const fl f011 = m_data(x0, y1, z1);
	const fl f111 = m_data(x1, y1, z1);

	const fl x = s[0];
	const fl y = s[1];
	const fl z = s[2];

	const fl mx = 1 - x;
	const fl my = 1 - y;
	const fl mz = 1 - z;

	fl f =
			f000 * mx * my * mz +
					f100 * x * my * mz +
					f010 * mx * y * mz +
					f110 * x * y * mz +
					f001 * mx * my * z +
					f101 * x * my * z +
					f011 * mx * y * z +
					f111 * x * y * z;

	if (deriv)
	{ // valid pointer
		const fl x_g =
				f000 * (-1) * my * mz +
						f100 * 1 * my * mz +
						f010 * (-1) * y * mz +
						f110 * 1 * y * mz +
						f001 * (-1) * my * z +
						f101 * 1 * my * z +
						f011 * (-1) * y * z +
						f111 * 1 * y * z;

		const fl y_g =
				f000 * mx * (-1) * mz +
						f100 * x * (-1) * mz +
						f010 * mx * 1 * mz +
						f110 * x * 1 * mz +
						f001 * mx * (-1) * z +
						f101 * x * (-1) * z +
						f011 * mx * 1 * z +
						f111 * x * 1 * z;

		const fl z_g =
				f000 * mx * my * (-1) +
						f100 * x * my * (-1) +
						f010 * mx * y * (-1) +
						f110 * x * y * (-1) +
						f001 * mx * my * 1 +
						f101 * x * my * 1 +
						f011 * mx * y * 1 +
						f111 * x * y * 1;

		vec gradient(x_g, y_g, z_g);
		curl(f, gradient, v);
		vec gradient_everywhere;

		VINA_FOR(i, 3)
		{
			gradient_everywhere[i] = ((region[i] == 0) ? gradient[i] : 0);
			(*deriv)[i] = m_factor[i] * gradient_everywhere[i]
					+ slope * region[i];
		}

		return f + penalty;
	}
	else
	{
		curl(f, v);
		return f + penalty;
	}
}
