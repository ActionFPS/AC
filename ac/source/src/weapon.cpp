// weapon.cpp: all shooting and effects code

#include "cube.h"

struct guninfo { short sound, reload, reloadtime, attackdelay,  damage, projspeed, part, spread, recoil, magsize, mdl_kick_rot, mdl_kick_back; char *name; };

const int SGRAYS = 20;  //down from 20 (defualt)
const float SGSPREAD = 2;
vec sg[SGRAYS];

//change the particales
guninfo guns[NUMGUNS] =
{    
    { S_KNIFE,    S_NULL,     0,      500,    50,     0,   0,  1,    1,   1,    0,  0,    "knife"   },

    { S_PISTOL,   S_RPISTOL,  1400,   170,    20,     0,   0, 80,   10,   8,    6,  5,  "pistol"  },  // *SGRAYS

    { S_SHOTGUN,  S_RSHOTGUN, 2400,   1000,   6,      0,   0,  1,   35,   7,    9,  9,  "shotgun" },  //reload time is for 1 shell from 7 too powerful to 6

    { S_SUBGUN,   S_RSUBGUN,  1650,   80,     17,     0,   0, 70,   15,   30,   1,  2,  "subgun"  },

    { S_SNIPER,   S_RSNIPER,  1950,   1500,   72,     0,   0, 60,   50,   5,    4,  4,  "sniper"  },

    { S_ASSULT,   S_RASSULT,  2000,   130,    20,     0,   0, 20,   40,   20,   0,  2,  "assult"  },  //recoil was 44

    { S_GRENADE,  S_NULL,     1000,   2000,   150,    20,   6,  1,    1,   1,    3,  1,  "grenade" },
};


bool gun_changed = false;

//weapon selection
void wup()
{
      if(player1->gunselect==GUN_KNIFE)
            player1->gunselect = GUN_PISTOL;
      else if(player1->gunselect==GUN_PISTOL && player1->primary!=GUN_PISTOL)
            player1->gunselect = player1->primary;
      else if(player1->gunselect==player1->primary)
            player1->gunselect=GUN_KNIFE;

      conoutf("%s selected", (int)guns[player1->gunselect].name);
      
      gun_changed = true;
};

void wdw()
{
      if(player1->gunselect==GUN_KNIFE)
            player1->gunselect = player1->primary;
      else if(player1->gunselect==player1->primary && player1->primary!=GUN_PISTOL)
            player1->gunselect = GUN_PISTOL;
      else if(player1->gunselect==GUN_PISTOL)
            player1->gunselect=GUN_KNIFE;

      conoutf("%s selected", (int)guns[player1->gunselect].name);
      
      gun_changed = true;
};

void primary()
{
      player1->gunselect=player1->primary;
      gun_changed = true;
};

void secondary()
{
      player1->gunselect=GUN_PISTOL;
      gun_changed = true;
};

void melee()
{
      player1->gunselect=GUN_KNIFE;
      gun_changed = true;
};

COMMAND(primary,ARG_NONE);

COMMAND(secondary,ARG_NONE);

COMMAND(melee,ARG_NONE);
COMMAND(wup,ARG_NONE);
COMMAND(wdw,ARG_NONE);

void reload()
{
      if(player1->gunselect==GUN_KNIFE || player1->gunselect==GUN_GRENADE) return;
      if(player1->mag[player1->gunselect]>=guns[player1->gunselect].magsize) return;
      if(player1->ammo[player1->gunselect]<=0) return;
      if(player1->reloading) return;

      player1->reloading = true;
      player1->lastaction = lastmillis;

      player1->gunwait = guns[player1->gunselect].reloadtime;
      
      //temp hack in drawhudgun() so no guns drawn while reloading, needs fixed

      int a = guns[player1->gunselect].magsize - player1->mag[player1->gunselect];
      if (a >= player1->ammo[player1->gunselect])
      {
            player1->mag[player1->gunselect] += player1->ammo[player1->gunselect];
            player1->ammo[player1->gunselect] = 0;
      }
      else
      {
            player1->mag[player1->gunselect] += a;
            player1->ammo[player1->gunselect] -= a;
      }

      playsoundc(guns[player1->gunselect].reload);
};

COMMAND(reload,ARG_NONE);

int reloadtime(int gun) { return guns[gun].reloadtime; };
int attackdelay(int gun) { return guns[gun].attackdelay; };
int magsize(int gun) { return guns[gun].magsize; };
int kick_rot(int gun) { return guns[gun].mdl_kick_rot; };
int kick_back(int gun) { return guns[gun].mdl_kick_back; };


void createrays(vec &from, vec &to)             // create random spread of rays for the shotgun
{
    vdist(dist, dvec, from, to);
    float f = dist*SGSPREAD/1000;
    loopi(SGRAYS)
    {
        #define RNDD (rnd(101)-50)*f
        vec r = { RNDD, RNDD, RNDD };
        sg[i] = to;
        vadd(sg[i], r); 
    };
};

bool intersect(dynent *d, vec &from, vec &to)   // if lineseg hits entity bounding box
{
    vec v = to, w = d->o, *p; 
    vsub(v, from);
    vsub(w, from);
    float c1 = dotprod(w, v);

    if(c1<=0) p = &from;
    else
    {
        float c2 = dotprod(v, v);
        if(c2<=c1) p = &to;
        else
        {
            float f = c1/c2;
            vmul(v, f);
            vadd(v, from);
            p = &v;
        };
    };

    return p->x <= d->o.x+d->radius
        && p->x >= d->o.x-d->radius
        && p->y <= d->o.y+d->radius
        && p->y >= d->o.y-d->radius
        && p->z <= d->o.z+d->aboveeye
        && p->z >= d->o.z-d->eyeheight;
};

dynent *playerincrosshair()
{
        if(demoplayback) return NULL;
    loopv(players)
    {
        dynent *o = players[i];
        if(!o) continue; 
        if(intersect(o, player1->o, worldpos)) return o;
    };
    return NULL;
};

const int MAXPROJ = 100;
struct projectile { vec o, to; float speed; dynent *owner; int gun; bool inuse, local; };
projectile projs[MAXPROJ];

void projreset() { loopi(MAXPROJ) projs[i].inuse = false; };

void newprojectile(vec &from, vec &to, float speed, bool local, dynent *owner, int gun)
{
    loopi(MAXPROJ)
    {
        projectile *p = &projs[i];
        if(p->inuse) continue;
        p->inuse = true;
        p->o = from;
        p->to = to;
        p->speed = speed;
        p->local = local;
        p->owner = owner;
        p->gun = gun;
        return;
    };
};

void hit(int target, int damage, dynent *d, dynent *at)
{
    if(d==player1) selfdamage(damage, at==player1 ? -1 : -2, at);
    else { addmsg(1, 4, SV_DAMAGE, target, damage, d->lifesequence); playsound(S_PAIN1+rnd(5), &d->o); };
    particle_splash(3, damage, 1000, d->o);
        demodamage(damage, d->o);
};

const float RL_RADIUS = 7;
const float RL_DAMRAD = 10;   // hack

void radialeffect(dynent *o, vec &v, int cn, int qdam, dynent *at)
{
    if(o->state!=CS_ALIVE) return;
    vdist(dist, temp, v, o->o);
    dist -= 2; // account for eye distance imprecision
    if(dist<RL_DAMRAD) 
    {
        if(dist<0) dist = 0;
        int damage = (int)(qdam*(1-(dist/RL_DAMRAD)));
        hit(cn, damage, o, at);
        vmul(temp, (RL_DAMRAD-dist)*damage/800);
        vadd(o->vel, temp);
    };
};

void splash(projectile *p, vec &v, vec &vold, int notthisplayer, int notthismonster, int qdam)
{
    particle_splash(0, 50, 300, v);
    p->inuse = false;
    if(p->gun!=GUN_GRENADE)
    {
        playsound(S_FEXPLODE, &v);
        // no push?
    }
    else
    {
        //playsound(S_RLHIT, &v);
        newsphere(v, RL_RADIUS, 0);
        dodynlight(vold, v, 0, 0, p->owner);
        if(!p->local) return;
        radialeffect(player1, v, -1, qdam, p->owner);
        loopv(players)
        {
            if(i==notthisplayer) continue;
            dynent *o = players[i];
            if(!o) continue; 
            radialeffect(o, v, i, qdam, p->owner);
        };
     };
};

inline void projdamage(dynent *o, projectile *p, vec &v, int i, int im, int qdam)
{
    if(o->state!=CS_ALIVE) return;
    if(intersect(o, p->o, v))
    {
        splash(p, v, p->o, i, im, qdam);
        hit(i, qdam, o, p->owner);
    }; 
};

void moveprojectiles(float time)
{
    loopi(MAXPROJ)
    {
        projectile *p = &projs[i];
        if(!p->inuse) continue;
        //int qdam = guns[p->gun].damage*(p->owner->quadmillis ? 4 : 1);
        int qdam = guns[p->gun].damage;
        vdist(dist, v, p->o, p->to);
        float dtime = dist*1000/p->speed;
        if(time>dtime) dtime = time;
        vmul(v, time/dtime);
        vadd(v, p->o)
        if(p->local)
        {
            loopv(players)
            {
                dynent *o = players[i];
                if(!o) continue; 
                projdamage(o, p, v, i, -1, qdam);
            };
            if(p->owner!=player1) projdamage(player1, p, v, -1, -1, qdam);
        };
        if(p->inuse)
        {
            if(time==dtime) splash(p, v, p->o, -1, -1, qdam);
            else
            {
                if(p->gun==GUN_GRENADE) { dodynlight(p->o, v, 0, 255, p->owner); particle_splash(5, 2, 200, v); }
                else { particle_splash(1, 1, 200, v); particle_splash(guns[p->gun].part, 1, 1, v); };
            };       
        };
        p->o = v;
    };
};

extern float rad(float x);

physent *curnade = NULL;

void throw_nade(dynent *d, vec &to, physent *p)
{
    if(!p || !d) return;
    printf("thrownade\n");

    p->isphysent = true;
    p->gravity = 20;
    p->state = NADE_THROWED;
    
    p->timeinair = 0;
    p->onfloor = false;
    
    p->o = d->o;
    p->vel.z = sin(rad(d->pitch));
    float speed = cos(rad(d->pitch));
    p->vel.x = sin(rad(d->yaw))*speed;
    p->vel.y = -cos(rad(d->yaw))*speed;
    
    vmul(p->vel, 1.7f);

    vec throwdir = p->vel;
    vmul(throwdir, d->radius);
    vadd(p->o, throwdir);
    vadd(p->o, throwdir);

    vec &from = d->o;
    
    if(d==player1)
    {
        d->thrownademillis = lastmillis;
        int percent_done = (lastmillis-p->millis)*100/2000;
        if(percent_done < 0 || percent_done > 100) percent_done = 100;
        addmsg(1, 9, SV_SHOT, d->gunselect, (int)(from.x*DMF), (int)(from.y*DMF), (int)(from.z*DMF), (int)(to.x*DMF), (int)(to.y*DMF), (int)(to.z*DMF), percent_done);
//        printf("grenade: %i\n", (lastmillis-curnade->millis)/100);
        if(curnade) curnade = NULL;
    };
};

physent *new_physent()
{
    physent *p = (physent *)gp()->alloc(sizeof(physent));
    
    p->yaw = 270;
    p->pitch = 0;
    p->roll = 0;
    p->isphysent = true;
    
    p->maxspeed = 40;
    p->outsidemap = false;
    p->inwater = false;
    p->radius = 0.2f;
    p->eyeheight = 0.3f;
    p->aboveeye = 0.0f;
    p->frags = 0;
    p->plag = 0;
    p->ping = 0;
    p->lastupdate = lastmillis;
    p->enemy = NULL;
    p->monsterstate = 0;
    p->name[0] = p->team[0] = 0;
    p->blocked = false;
    p->lifesequence = 0;
    p->dynent::state = CS_ALIVE;
    p->state = PHYSENT_NONE;
    p->shots = 0;
    p->reloading = false;
    p->nextprimary = 1;
    p->hasarmour = false;

    p->k_left = false;
    p->k_right = false;
    p->k_up = false;
    p->k_down = false;  
    p->jumpnext = false;
    p->onladder = false;
    p->strafe = 0;
    p->move = 0;
    
    physents.add(p);
    return p;
}

physent *new_nade(dynent *d, int millis = 0)
{
    printf("newnade\n");
    physent *p = new_physent();
    p->owner = d;
    p->millis = lastmillis;
    p->timetolife = 2000-millis;
    p->state = NADE_ACTIVATED;
    if(d==player1) 
    {
        curnade = p;
        d->thrownademillis = 0;
    };
    return p;
};

void explode_nade(physent *i)
{ 
    if(!i) return;
    printf("explode\n");
    
    if(i->state != NADE_THROWED)
    {   
        vec o = i->owner->o;
        vec dist = { 0.1f, 0.1f, 0.1f };
        vadd(o, dist);
        throw_nade(i->owner, o, i);
    };
    newprojectile(i->o, i->o, 1, true, i->owner, GUN_GRENADE);
};

void shootv(int gun, vec &from, vec &to, dynent *d, bool local, int nademillis)     // create visual effect from a shot
{
    playsound(guns[gun].sound, d==player1 ? NULL : &d->o);
    int pspeed = 25;
    switch(gun)
    {
        case GUN_KNIFE:
            break;

        case GUN_SHOTGUN:
        {
            loopi(SGRAYS) particle_splash(0, 5, 200, sg[i]);
            break;
        };

        case GUN_PISTOL:
        case GUN_SUBGUN:
        case GUN_ASSULT:
            addshotline(d, from, to);
            particle_splash(9, 5, 250, to);
            //particle_trail(1, 10, from, to);
            break;

        case GUN_GRENADE:
            //pspeed = guns[gun].projspeed;
            //newprojectile(from, to, (float)pspeed, local, d, gun);

            if(d!=player1)
            {
                physent *p = new_nade(d, nademillis);
                throw_nade(d, to, p);
            }
            
            break;

        case GUN_SNIPER: 
            addshotline(d, from, to);
            particle_splash(0, 50, 200, to);
            particle_trail(1, 500, from, to);
            break;
    };
};

void hitpush(int target, int damage, dynent *d, dynent *at, vec &from, vec &to)
{
    hit(target, damage, d, at);
    vdist(dist, v, from, to);
    vmul(v, damage/dist/50);
    vadd(d->vel, v);
};

void raydamage(dynent *o, vec &from, vec &to, dynent *d, int i)
{
    if(o->state!=CS_ALIVE) return;
    int qdam = guns[d->gunselect].damage;
    //if(d->quadmillis) qdam *= 4;
    if(d->gunselect==GUN_SHOTGUN)
    {
        int damage = 0;
        loop(r, SGRAYS) if(intersect(o, from, sg[r])) damage += qdam;
        if(damage) hitpush(i, damage, o, d, from, to);
    }
    else if(intersect(o, from, to)) hitpush(i, qdam, o, d, from, to);
};
void spreadandrecoil(vec & from, vec & to, dynent * d)
{
    //nothing special for a knife
    if (d->gunselect==GUN_KNIFE || d->gunselect==GUN_GRENADE) return;

    //spread
    vdist(dist, unitv, from, to);
    float f = dist/1000;
    int spd = guns[d->gunselect].spread;

    //recoil
    int rcl = guns[d->gunselect].recoil*-0.01f;

    if (d->gunselect==GUN_ASSULT)
    {
        if(d->shots > 3)
            spd = 70;
        rcl += (rnd(8)*-0.01f);
    };

    if ((d->gunselect==GUN_SNIPER) && (d->vel.x<.25f && d->vel.y<.25f))
    {
        spd = 1;
        rcl = rcl / 3;
    };

    if (d->gunselect!=GUN_SHOTGUN)  //no spread on shotgun
    {   
        #define RNDD (rnd(spd)-spd/2)*f
        vec r = { RNDD, RNDD, RNDD };
        vadd(to, r);
    };

   //increase pitch for recoil
    vdiv(unitv, dist);
    vec recoil = unitv;
    vmul(recoil, rcl);
    vadd(d->vel, recoil);

    if(d->pitch<80.0f) d->pitch += guns[d->gunselect].recoil*0.05f;
};

void shoot(dynent *d, vec &targ)
{   
    int attacktime = lastmillis-d->lastaction;
    
    if(!d->attacking && d->gunselect==GUN_GRENADE && curnade) // throw
    {
        throw_nade(d, targ, curnade);
        return;
    }
    
    if(attacktime<d->gunwait) return;
    d->gunwait = 0;
    d->reloading = false;

    if(!d->attacking) 
    { 
        d->shots = 0;
        return; 
    };
    
    if(d->gunselect!=GUN_SUBGUN && d->gunselect!=GUN_ASSULT && d->gunselect!=GUN_GRENADE) d->attacking = false;  //makes sub/assult autos
    else d->shots++;

    d->lastaction = lastmillis;
    d->lastattackgun = d->gunselect;
    if(!d->mag[d->gunselect]) { playsoundc(S_NOAMMO); d->gunwait = 250; d->lastattackgun = -1; return; };

    if(d->gunselect) d->mag[d->gunselect]--;

    vec from = d->o;
    vec to = targ;
    from.z -= 0.2f;    // below eye
    
    spreadandrecoil(from,to,d);

    vdist(dist, unitv, from, to);
    vdiv(unitv, dist);

    if(d->gunselect==GUN_KNIFE) 
    {
        vmul(unitv, 3); // punch range
        to = from;
        vadd(to, unitv);
    };   
    if(d->gunselect==GUN_SHOTGUN) createrays(from, to);

    //if(d->quadmillis && attacktime>200) playsoundc(S_ITEMPUP);
    
    d->gunwait = guns[d->gunselect].attackdelay;
    
    if(d->gunselect==GUN_GRENADE) // activate
    {
        if(!curnade) new_nade(player1);
        return;
    }
    
    shootv(d->gunselect, from, to, d, 0);
    addmsg(1, 9, SV_SHOT, d->gunselect, (int)(from.x*DMF), (int)(from.y*DMF), (int)(from.z*DMF), (int)(to.x*DMF), (int)(to.y*DMF), (int)(to.z*DMF), 0 );

    if(guns[d->gunselect].projspeed) return;
    
    loopv(players)
    {
        dynent *o = players[i];
        if(!o) continue; 
        raydamage(o, from, to, d, i);
    };

};
