import { Injectable } from '@angular/core';
import { BaseService } from './base.service';
import { HttpClient, HttpParams } from '@angular/common/http';
import { Observable } from 'rxjs';
import { SettingsKey, SettingsDesc } from './settings';

@Injectable({
  providedIn: 'root'
})
export class SettingsService extends BaseService {

  constructor(http: HttpClient) {
    super("SettingsService", http);
  }

  public get(path: string): Observable<Map<String,SettingsKey>> {
    return this.httpGet('/settings' + path);
  }
  public desc(path: string): Observable<SettingsDesc[]> {
    return this.httpGet('/settings/descriptions' + path, { params: new HttpParams().set('samples', 'true')});
  }
}
