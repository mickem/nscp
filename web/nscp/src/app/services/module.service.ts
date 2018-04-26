import { Injectable } from '@angular/core';
import { HttpClient } from '@angular/common/http';
import { Observable } from 'rxjs';
import { BaseService } from './base.service';
import { Module } from './module';

@Injectable()
export class ModuleService extends BaseService {

  constructor(http: HttpClient) {
    super("ModuleService", http);
  }

  public list(): Observable<Module[]> {
    return this.httpGet('/modules?all=true');
  }
  public load(name: string): Observable<String> {
    return this.httpGet(`/modules/${name}/commands/load`);
  }
  public unload(name: string): Observable<String> {
    return this.httpGet(`/modules/${name}/commands/unload`);
  }
  public enable(name: string): Observable<String> {
    return this.httpGet(`/modules/${name}/commands/enable`);
  }
  public disable(name: string): Observable<String> {
    return this.httpGet(`/modules/${name}/commands/disable`);
  }
}
