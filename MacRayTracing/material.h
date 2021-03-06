#ifndef _MATERIAL_h_
#define _MATERIAL_h_

#include "ray3.h"
#include "trace_record.h"
#include "texture.h"

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class material
{
public:
	material()
	{
	}

	virtual bool scatter_pdf(const ray3& in, const trace_record& record, ray3& scattered)
	{
		float cosine = dot(record.normal, unit_vector(scattered.direction()));
		if (cosine < 0)
			cosine = 0;
		
		return cosine / ONE_PI;
	}

	virtual bool scatter(const ray3& in, const trace_record& record, vec3& albedo, ray3& scattered, float &pdf) const = 0;

	virtual vec3 emit(float u, float v, const vec3& p) const = 0;
};

class isotropic : public material 
{
public:
	isotropic(shared_ptr<texture> albedo_)
	: albedo(albedo_)
	{
	}

	virtual bool scatter(const ray3& in, const trace_record& record, vec3& attenuation, ray3& scattered, float& pdf) const
	{
		scattered = ray3(record.position, random_in_unit_sphere(), in.time());
		attenuation = albedo->sample(record.texcoord, record.position);
		return true;
	}

	virtual vec3 emit(float u, float v, const vec3& p) const
	{
		return vec3(0, 0, 0);
	}
	
	shared_ptr<texture> albedo;
};

class lambertian : public material
{
public:
	lambertian(shared_ptr<texture> albedo)
	{
		this->albedo = albedo;
	}

	virtual bool scatter(const ray3& in, const trace_record& record, vec3& albedo, ray3& scattered, float& pdf) const
	{
		vec3 target = record.position + record.normal + random_in_unit_sphere();
		
		scattered = ray3(record.position, target - record.position, in.time());
		
		albedo = this->albedo->sample(record.texcoord, record.position);

		pdf = dot(record.normal, unit_vector(scattered.direction())) / ONE_PI;

		return true;
	}

	virtual vec3 emit(float u, float v, const vec3& p) const
	{
		return vec3(0, 0, 0);
	}

	shared_ptr<texture> albedo;
};

class transmissive : public material
{
public:
	transmissive(shared_ptr<texture> albedo, float ior)
	{
		this->albedo = albedo;
		this->ior = ior;
	}

	float schlick(float cosine, float ior) const
	{
		auto r0 = (1 - ior) / (1 + ior);
		r0 = r0 * r0;
		return r0 + (1 - r0) * pow((1 - cosine), 5);
	}

	vec3 reflect(const vec3& v, const vec3& n) const
	{
		return v - 2 * dot(v, n) * n;
	}

	bool refract(const vec3& v, const vec3& n, float ni_over_nt, vec3& refracted) const
	{
		vec3 uv = unit_vector(v);
		float dt = dot(uv, n);
		float discriminant = 1.0 - ni_over_nt * ni_over_nt * (1.0 - dt * dt);
		if (discriminant > 0)
		{
			refracted = ni_over_nt * (uv - n * dt) - n * sqrt(discriminant);
			return true;
		}
		else
			return false;
	}

	virtual bool scatter2(const ray3& in, const trace_record& record, vec3& attenuation, ray3& scattered, float& pdf) const
	{
		vec3 outward_normal;
		vec3 reflected = reflect(in.direction(), record.normal);
		float ni_over_nt;
		vec3 refracted;
		float cosine;
		float reflect_prob;
		attenuation = this->albedo->sample(record.texcoord, record.position);
		pdf = 1;

		if (dot(in.direction(), record.normal) > 0) // ray hit from inside
		{
			outward_normal = -record.normal;
			ni_over_nt = ior;

			cosine = ior * dot(in.direction(), record.normal) / in.direction().length();
		}
		else
		{
			outward_normal = record.normal;
			ni_over_nt = 1.0 / ior;

			cosine = -dot(in.direction(), record.normal) / in.direction().length();
		}

		if (refract(in.direction(), outward_normal, ni_over_nt, refracted))
		{
			reflect_prob = schlick(cosine, ior);
		}
		else
		{
			reflect_prob = 1.0;
		}

		if (random() < reflect_prob)
		{
			scattered = ray3(record.position, reflected, in.time());
		}
		else
		{
			scattered = ray3(record.position, refracted, in.time());
		}

		return true;
	}

	virtual bool scatter(const ray3& in, const trace_record& record, vec3& attenuation, ray3& scattered, float& pdf) const
	{
		return scatter2(in, record, attenuation, scattered, pdf);
	}

	virtual vec3 emit(float u, float v, const vec3& p) const
	{
		return vec3(0, 0, 0);
	}

	shared_ptr<texture> albedo;
	float ior;
};

class metal : public material
{
public:
	metal(const vec3& albedo, float fuzz = 0.0)
	{
		this->albedo = albedo;
		this->fuzz = fuzz;
	}

	vec3 reflect(const vec3& v, const vec3& n) const
	{
		return v - 2 * dot(v, n) * n;
	}

	virtual bool scatter(const ray3& in, const trace_record& record, vec3& atteunation, ray3& scattered, float& pdf) const
	{
		vec3 reflected = reflect(unit_vector(in.direction()), record.normal);
		scattered = ray3(record.position, reflected + fuzz*random_in_unit_sphere(), in.time());
		atteunation = albedo;

		pdf = 1;
		
		return dot(scattered.direction(), record.normal)>0;
	}

	virtual vec3 emit(float u, float v, const vec3& p) const
	{
		return vec3(0, 0, 0);
	}

	vec3 albedo;
	float fuzz;
};

class dielectric : public material
{
public:
	dielectric(vec3 albedo, float ior)
	{
		this->albedo = albedo;
		this->ior = ior;
	}

	float schlick(float cosine, float ior) const
	{
		auto r0 = (1 - ior) / (1 + ior);
		r0 = r0 * r0;
		return r0 + (1 - r0) * pow((1 - cosine), 5);
	}

	vec3 reflect(const vec3& v, const vec3& n) const
	{
		return v - 2 * dot(v, n) * n;
	}

	bool refract(const vec3& v, const vec3& n, float ni_over_nt, vec3& refracted) const
	{
		vec3 uv = unit_vector(v);
		float dt = dot(uv, n);
		float discriminant = 1.0 - ni_over_nt * ni_over_nt * (1.0 - dt * dt);
		if (discriminant > 0)
		{
			refracted = ni_over_nt * (uv - n * dt) - n * sqrt(discriminant);
			return true;
		}
		else
			return false;
	}

	virtual bool scatter2(const ray3& in, const trace_record& record, vec3& attenuation, ray3& scattered, float& pdf) const
	{
		vec3 outward_normal;
		vec3 reflected = reflect(in.direction(), record.normal);
		float ni_over_nt;
		vec3 refracted;
		float cosine;
		float reflect_prob;
		attenuation = albedo;
		pdf = 1;

		if (dot(in.direction(), record.normal) > 0) // ray hit from inside
		{
			outward_normal = -record.normal;
			ni_over_nt = ior;
			
			cosine = ior * dot(in.direction(), record.normal) / in.direction().length();
		}
		else
		{
			outward_normal = record.normal;
			ni_over_nt = 1.0 / ior;

			cosine = -dot(in.direction(), record.normal) / in.direction().length();
		}

		if (refract(in.direction(), outward_normal, ni_over_nt, refracted))
		{
			reflect_prob = schlick(cosine, ior);
		}
		else
		{
			reflect_prob = 1.0;
		}

		if (random() < reflect_prob)
		{
			scattered = ray3(record.position, reflected, in.time());
		}
		else
		{
			scattered = ray3(record.position, refracted, in.time());
		}

		return true;
	}

	virtual bool scatter(const ray3& in, const trace_record& record, vec3& attenuation, ray3& scattered, float& pdf) const
	{
		return scatter2(in, record, attenuation, scattered, pdf);
	}

	virtual vec3 emit(float u, float v, const vec3& p) const
	{
		return vec3(0, 0, 0);
	}

	vec3 albedo;
	float ior;
};

class diffuse_light : public material
{
public:
	diffuse_light(const vec3& color_)
	{
		color = color_;
	}

	virtual bool scatter(const ray3& in, const trace_record& record, vec3& attenuation, ray3& scattered, float& pdf) const
	{
		return false;
	}

	virtual vec3 emit(float u, float v, const vec3& p) const
	{
		return color;
	}

	vec3 color;
};

class diffuse_texlight : public material
{
public:
	diffuse_texlight(shared_ptr<texture> color_)
	{
		color = color_;
	}

	virtual bool scatter(const ray3& in, const trace_record& record, vec3& attenuation, ray3& scattered, float& pdf) const
	{
		return false;
	}

	virtual vec3 emit(const vec3& texcoord, const vec3& position) const
	{
		return color->sample(texcoord, position);
	}

	shared_ptr<texture> color;
};

#endif